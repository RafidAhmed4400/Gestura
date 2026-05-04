#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include "driver/adc.h"

#include "gesture_model.h"
#include "bno055.h"
#include "flex_sensor.h"

static uint8_t s_led_state = 0;

static led_strip_handle_t led_strip;

static const char *TAG = "GESTURE_APP";

// IMU BNO055 chip settings
#define CONFIG_BNO055_SCL_PIN 9
#define CONFIG_BNO055_SDA_PIN 18
#define CONFIG_BNO055_I2C_ADDR 0x28
#define CONFIG_BNO055_I2C_FREQUENCY 400

// LED Blink constants
#define CONFIG_BLINK_PERIOD 1000
#define CONFIG_DATA_PERIOD 10
#define BLINK_GPIO 38

// NN Model settings
#define NUM_TIMESTEPS 110
#define NUM_FEATURES 14 // 5 flex sensors + 9 IMU features

#define SAMPLE_DELAY_MS 10   // 100 Hz sampling rate

// ADC channel mapping for flex sensors
#define FLEX_THUMB_CHANNEL   ADC1_CHANNEL_0
#define FLEX_INDEX_CHANNEL   ADC1_CHANNEL_9
#define FLEX_MIDDLE_CHANNEL  ADC1_CHANNEL_6
#define FLEX_RING_CHANNEL    ADC1_CHANNEL_7
#define FLEX_PINKY_CHANNEL   ADC1_CHANNEL_2

// Gesture buffer
// Shape: 110 timesteps x 14 sensor features
// Feature order:
// 0 thumb
// 1 index
// 2 middle
// 3 ring
// 4 pinky
// 5 gx
// 6 gy
// 7 gz
// 8 ax
// 9 ay
// 10 az
// 11 grav_x
// 12 grav_y
// 13 grav_z
static float gesture_window[NUM_TIMESTEPS][NUM_FEATURES];

// function prototypes
static void collect_gesture_window(bno055_t* bno055, flex_data_t* flex_data);
static void print_gesture_window_csv(int gesture_id, const char *label);
static void run_gesture_classification(void);

static void blink_led(void);
static void configure_led(void);

// Main app
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(BNO055_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(DATA_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(FSR_TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Gesture recognition app starting...");

    /* imu setup */
    bno055_t bno055 = {0};
    i2c_master_bus_config_t i2c_master_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM_0,
        .intr_priority = 0,
        .scl_io_num = CONFIG_BNO055_SCL_PIN,
        .sda_io_num = CONFIG_BNO055_SDA_PIN,
        .trans_queue_depth = 0,
    };
    i2c_master_bus_handle_t i2c_master_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_conf, &i2c_master_bus));
    i2c_device_config_t bno055_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_BNO055_I2C_ADDR,
        .flags.disable_ack_check = 0,
        .scl_speed_hz = CONFIG_BNO055_I2C_FREQUENCY * 1000,
        .scl_wait_us = 0xffff,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_master_bus, &bno055_conf, &bno055.config.slave_handle));

    ESP_ERROR_CHECK(bno055_initialize(&bno055));
    ESP_LOGI("MAIN", "BNO055 initialized");

    ESP_ERROR_CHECK(bno055_configure(&bno055, NDOF_MODE, (ACC_MG | GY_RPS | EUL_DEG)));

    /* imu calibration */
    ESP_LOGI("BNO055", "Calibrating the sensor, please move the sensor");
    while (1)
    {
        ESP_ERROR_CHECK(bno055_get_calibration_status(&bno055));
        if (bno055.config.is_calibrated)
            break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI("BNO055", "Calibration done");
    esp_log_level_set(BNO055_TAG, ESP_LOG_INFO);

    /* flex sensor stuff */
    flex_data_t flex_data = {0};
    ESP_ERROR_CHECK(flex_init());

    /* Configure the peripheral according to the LED type */
    configure_led();

    /* gesture model stuff */
    gesture_model_init();

    ESP_LOGI(TAG, "Starting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        ESP_LOGI(TAG, "Collecting gesture window...");

        collect_gesture_window(&bno055, &flex_data);

        ESP_LOGI(TAG, "Classifying gesture...");

        run_gesture_classification();

        ESP_LOGI(TAG, "Waiting before next classification...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Collect one gesture window
// 110 samples

static void collect_gesture_window(bno055_t* bno055, flex_data_t* flex_data)
{
    for (int t = 0; t < NUM_TIMESTEPS; t++) {

        flex_read_normalized(flex_data);

        /* imu readings */
        bno055_get_readings(bno055, GYROSCOPE);
        bno055_get_readings(bno055, LINEAR_ACCELERATION);
        bno055_get_readings(bno055, GRAVITY);

        gesture_window[t][0] = bno055->gyroscope.x;
        gesture_window[t][1] = bno055->gyroscope.y;
        gesture_window[t][2] = bno055->gyroscope.z;

        gesture_window[t][3] = bno055->linear_acceleration.x;
        gesture_window[t][4] = bno055->linear_acceleration.y;
        gesture_window[t][5] = bno055->linear_acceleration.z;

        
        gesture_window[t][6] = bno055->gravity.x;
        gesture_window[t][7] = bno055->gravity.y;
        gesture_window[t][8] = bno055->gravity.z;
            
        gesture_window[t][9] = (float)flex_data->fsr_pinky;
        gesture_window[t][10] = (float)flex_data->fsr_ring;
        gesture_window[t][11] = (float)flex_data->fsr_middle;
        gesture_window[t][12] = (float)flex_data->fsr_index;
        gesture_window[t][13] = (float)flex_data->fsr_thumb;



        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }
}


// ------------------------------------------------------
// Optional: print the gesture window as CSV
//
// Use this when collecting training data.
// Example:
//
// gesture_id,label,timestep,timestamp_ms,thumb,index,middle,ring,pinky,gx,gy,gz,ax,ay,az,qx,qy,qz,qw
//
// You can copy the serial output into a CSV file.
// ------------------------------------------------------

static void print_gesture_window_csv(int gesture_id, const char *label)
{
    for (int t = 0; t < NUM_TIMESTEPS; t++) {
        int timestamp_ms = t * SAMPLE_DELAY_MS;

        printf(
            "%d,%s,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
            gesture_id,
            label,
            t,
            timestamp_ms,
            gesture_window[t][0],
            gesture_window[t][1],
            gesture_window[t][2],
            gesture_window[t][3],
            gesture_window[t][4],
            gesture_window[t][5],
            gesture_window[t][6],
            gesture_window[t][7],
            gesture_window[t][8],
            gesture_window[t][9],
            gesture_window[t][10],
            gesture_window[t][11],
            gesture_window[t][12],
            gesture_window[t][13]
        );
    }
}


// Helper function to run gesture classification

static void run_gesture_classification(void)
{
    int predicted_class = gesture_model_predict(gesture_window);

    const char *gesture_name = gesture_model_get_class_name(predicted_class);

    ESP_LOGI(TAG, "Predicted class index: %d", predicted_class);
    ESP_LOGI(TAG, "Predicted gesture: %s", gesture_name);
}

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };

    // config for RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // config for SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));

    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}