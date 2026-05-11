#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2s_std.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "gesture_model.h"
#include "bno055.h"
#include "flex_sensor.h"
#include "lookup_table.h"

#include "speech.h"   // all audio lives here

static uint8_t s_led_state = 0;

static led_strip_handle_t led_strip;

static i2s_chan_handle_t tx_handle = NULL;

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

// I2S pin assignments
#define I2S_BCLK_GPIO   4
#define I2S_WS_GPIO     5
#define I2S_DOUT_GPIO   6
#define SAMPLE_RATE     48000
#define DMA_DESC_NUM    4
#define DMA_FRAME_NUM   128

// Gesture buffer
// Shape: 110 timesteps x 14 sensor features
// Feature order:
// 0 gx
// 1 gy
// 2 gz
// 3 ax
// 4 ay
// 5 az
// 6 grav_x
// 7 grav_y
// 8 grav_z
// 9 pinky 
// 10 ring
// 11 middle
// 12 index
// 13 thumb
static float gesture_window[NUM_TIMESTEPS][NUM_FEATURES];

// Define a custom service and characteristic UUIDs in little endian format
static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0xC5, 0xB4, 0xA3, 0xF2, 0xE1, 0xD0, 0xC9, 0xB8,
                     0xA7, 0xF6, 0xE5, 0xD4, 0xC3, 0xB2, 0xE1, 0xA0);

static const ble_uuid128_t gesture_char_uuid =
    BLE_UUID128_INIT(0xC6, 0xB5, 0xA4, 0xF3, 0xE2, 0xD1, 0xC0, 0xB9,
                     0xA8, 0xF7, 0xE6, 0xD5, 0xC4, 0xB3, 0xE2, 0xA1);
// define the characteristic handle and connection handle
static uint16_t gesture_val_handle;
static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
// define flags to track connection and notification status
static bool device_connected = false;
static bool notify_enabled = false;
// buffer to hold the current gesture text
static char gesture_text[64] = "READY";
static char last_sent_text[64] = "";

// function prototypes
static void collect_gesture_window(bno055_t* bno055, flex_data_t* flex_data);
static void print_gesture_window_csv(int gesture_id, const char *label);
static int run_gesture_classification(void);

static void blink_led(void);
static void configure_led(void);

static esp_err_t audio_i2s_init(void);

static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void start_advertising(void);
static int gesture_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static void on_stack_sync(void);
static void host_task(void *param);
static void send_gesture_notification(const char *msg);
static void ble_send_string(const char* msg);
static void ble_init(void);

// define the GATT service and characteristic
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {   
        // defines a primary service with the specified UUID and a single characteristic for gesture text
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   
                // defines a characteristic with the specified UUID, read and notify properties, and the access callback function
                .uuid = &gesture_char_uuid.u,
                .access_cb = gesture_chr_access_cb,
                .val_handle = &gesture_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0}
        },
    },
    {0}
};

// Main app
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(BNO055_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(DATA_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(FSR_TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Gesture recognition app starting...");

    /* configure bluetooth */
    ble_init();

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

    /* configure speech */
    ESP_ERROR_CHECK(audio_i2s_init());
    speech_init(&tx_handle);

    /* gesture model stuff */
    gesture_model_init();

    ESP_LOGI(TAG, "Starting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        ESP_LOGI(TAG, "Collecting gesture window...");

        collect_gesture_window(&bno055, &flex_data);

        ESP_LOGI(TAG, "Classifying gesture...");

        const int pred_class = run_gesture_classification();
        ble_send_string(letters[pred_class]);
        speech_play(letters[pred_class]);
        ESP_LOGI(TAG, "Waiting before next classification...");
        vTaskDelay(pdMS_TO_TICKS(10));
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

// print the gesture window as CSV
// Use this when collecting training data.
// You can copy the serial output into a CSV file.

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

static int run_gesture_classification(void)
{
    int predicted_class = gesture_model_predict(gesture_window);

    const char *gesture_name = gesture_model_get_class_name(predicted_class);

    ESP_LOGI(TAG, "Predicted class index: %d", predicted_class);
    ESP_LOGI(TAG, "Predicted gesture: %s", gesture_name);
    return predicted_class;
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

// Initialize the I2S audio interface
static esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    return ESP_OK;
}

// GATT access callback function for the gesture characteristic
static int gesture_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // prevent unused parameter warnings, not strictly necessary
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;


    switch (ctxt->op) {
        // case for read operation on the characteristic, return the current gesture text
        case BLE_GATT_ACCESS_OP_READ_CHR:
            // ctxt -> om is the outgoing message buffer, that gets appended with gesture_text, strlen() is just length of text
            if (os_mbuf_append(ctxt->om, gesture_text, strlen(gesture_text)) != 0) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            // return success
            return 0;

        default:
            // return generic ble error
            return BLE_ATT_ERR_UNLIKELY;
    }
}

// function to start advertising the BLE service with the defined UUID and characteristics
static void start_advertising(void)
{
    struct ble_hs_adv_fields adv_fields; // advertisement packet 
    struct ble_hs_adv_fields rsp_fields; // scan response packet
    struct ble_gap_adv_params adv_params; // advertising parameters
    uint8_t own_addr_type; 
    int rc; // return code for error handling

    rc = ble_hs_id_infer_auto(0, &own_addr_type); // infer the device's own address type, 0 means no privacy, will use public address
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }
    
    // set up the advertisement fields, including flags, service UUID, and device name
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    // set up the scan response fields, including the device name
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    const char *name = ble_svc_gap_device_name();
    rsp_fields.name = (const uint8_t *)name;
    rsp_fields.name_len = strlen(name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    // set up the advertising parameters, including connection mode, discoverability, and intervals
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started");
    }
}
// callback function to handle GAP events such as connection, disconnection, subscription changes, and MTU updates
static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        // handle connection event, update connection status and log the event
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                current_conn_handle = event->connect.conn_handle;
                device_connected = true;
                notify_enabled = false;
                ESP_LOGI(TAG, "Connected, conn_handle=%d", current_conn_handle);
            } else {
                ESP_LOGW(TAG, "Connect failed, status=%d", event->connect.status);
                start_advertising();
            }
            return 0;
        
        // handle disconnection event, reset connection status and restart advertising
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
            current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            device_connected = false;
            notify_enabled = false;
            start_advertising();
            return 0;
        
        // handle subscription changes for notifications, update the notify_enabled flag and log the new subscription status
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == gesture_val_handle) {
                notify_enabled = event->subscribe.cur_notify;
                ESP_LOGI(TAG,
                         "Subscribe changed: notify=%d indicate=%d",
                         event->subscribe.cur_notify,
                         event->subscribe.cur_indicate);
            }
            return 0;

        // handle MTU (Maximum Transmission Unit) update event, log the new MTU value for the connection 
        // MTU determines the maximum size of data that can be sent in a single BLE packet
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG,
                     "MTU updated: conn_handle=%d mtu=%d",
                     event->mtu.conn_handle,
                     event->mtu.value);
            return 0;

        default:
            return 0;
    }
}

// callback function to handle stack synchronization, ensures the device has a valid address and starts advertising
static void on_stack_sync(void) {
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }

    start_advertising();
}

// FreeRTOS task to run the NimBLE host stack, this is required for handling BLE events and operations
static void host_task(void *param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// helper function to send a notification with the current gesture text to the connected client
// checks connection and notification status before sending
static void send_gesture_notification(const char *msg) {
    if (!device_connected || !notify_enabled) {
        return;
    }

    if (msg == NULL || msg[0] == '\0') {
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(msg, strlen(msg));
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate notification buffer");
        return;
    }

    int rc = ble_gatts_notify_custom(current_conn_handle, gesture_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_notify_custom failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Notification sent: %s", msg);
    }
}

// function to send string over bluetooth, with checks and keeps track of last send message
static void ble_send_string(const char* msg) {
    if (device_connected &&
        notify_enabled &&
        strncmp(msg, last_sent_text, sizeof(last_sent_text)) != 0) {
            send_gesture_notification(msg);

            strncpy(last_sent_text, msg, sizeof(last_sent_text) - 1);
            last_sent_text[sizeof(last_sent_text) - 1] = '\0';
        }
}

// main initialization function to set up BLE, including NVS flash, Bluetooth controller, NimBLE stack, GAP and GATT services
static void ble_init(void) {
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set("Gesture Recognition");
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        return;
    }

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb = on_stack_sync;

    nimble_port_freertos_init(host_task);
}