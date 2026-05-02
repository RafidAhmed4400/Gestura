#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "driver/adc.h"

#include "gesture_model.h"

static const char *TAG = "GESTURE_APP";

// NN Model settings
#define NUM_TIMESTEPS 140
#define NUM_FEATURES 15 // 5 flex sensors + 10 IMU features

#define SAMPLE_DELAY_MS 10   // 100 Hz sampling rate

// ADC channel mapping for flex sensors
#define FLEX_THUMB_CHANNEL   ADC1_CHANNEL_0
#define FLEX_INDEX_CHANNEL   ADC1_CHANNEL_9
#define FLEX_MIDDLE_CHANNEL  ADC1_CHANNEL_6
#define FLEX_RING_CHANNEL    ADC1_CHANNEL_7
#define FLEX_PINKY_CHANNEL   ADC1_CHANNEL_2

// Gesture buffer
// Shape: 140 timesteps x 15 sensor features
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
// 11 qx 
// 12 qy 
// 13 qz 
// 14 qw 
static float gesture_window[NUM_TIMESTEPS][NUM_FEATURES];

// function prototypes
static void adc_init(void);
static int read_flex_adc(adc1_channel_t channel);
static void read_all_flex_sensors(int *thumb, int *index, int *middle, int *ring, int *pinky);
static void read_imu(float *gx, float *gy, float *gz, float *ax, float *ay, float *az, float *qx, float *qy, float *qz, float *qw);

static void collect_gesture_window(void);
static void print_gesture_window_csv(int gesture_id, const char *label);
static void run_gesture_classification(void);


// Main app
void app_main(void)
{
    ESP_LOGI(TAG, "Gesture recognition app starting...");

    adc_init();

    gesture_model_init();

    ESP_LOGI(TAG, "Starting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        ESP_LOGI(TAG, "Collecting gesture window...");

        collect_gesture_window();

        ESP_LOGI(TAG, "Classifying gesture...");

        run_gesture_classification();

        ESP_LOGI(TAG, "Waiting before next classification...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ADC setup

static void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(FLEX_THUMB_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FLEX_INDEX_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FLEX_MIDDLE_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FLEX_RING_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FLEX_PINKY_CHANNEL, ADC_ATTEN_DB_11);

    ESP_LOGI(TAG, "ADC initialized.");
}

// helper function to read a single flex sensor
static int read_flex_adc(adc1_channel_t channel)
{
    return adc1_get_raw(channel);
}

// helper function to read all flex sensors
static void read_all_flex_sensors(
    int *thumb,
    int *index,
    int *middle,
    int *ring,
    int *pinky
)
{
    *thumb = read_flex_adc(FLEX_THUMB_CHANNEL);
    *index = read_flex_adc(FLEX_INDEX_CHANNEL);
    *middle = read_flex_adc(FLEX_MIDDLE_CHANNEL);
    *ring = read_flex_adc(FLEX_RING_CHANNEL);
    *pinky = read_flex_adc(FLEX_PINKY_CHANNEL);
}


// ------------------------------------------------------
// Read IMU
//
// Replace this function with your actual IMU driver.
// For example, if you are using MPU6050, ICM20948, MPU9250,
// etc., this is where you call that library.
//
// For now, this dummy version returns still-hand values.
// ------------------------------------------------------

static void read_imu(
    float *gx,
    float *gy,
    float *gz,
    float *ax,
    float *ay,
    float *az,
    float *qx,
    float *qy,
    float *qz,
    float *qw
)
{
    // Dummy values.
    // Replace these with real readings from your IMU driver.
    // Expected IMU output order:
    // g_x, g_y, g_z, a_x, a_y, a_z, x, y, z, w

    *gx = 0.0f;
    *gy = 0.0f;
    *gz = 0.0f;

    *ax = 0.0f;
    *ay = 0.0f;
    *az = 9.81f;

    // Quaternion orientation. A stationary neutral orientation is usually identity: x=0, y=0, z=0, w=1.
    *qx = 0.0f;
    *qy = 0.0f;
    *qz = 0.0f;
    *qw = 1.0f;
}


// Collect one gesture window
// 140 samples
// 20 ms between samples
// total duration = 1.5 seconds

static void collect_gesture_window(void)
{
    for (int t = 0; t < NUM_TIMESTEPS; t++) {
        int thumb, index, middle, ring, pinky;
        float gx, gy, gz, ax, ay, az, qx, qy, qz, qw;

        read_all_flex_sensors(&thumb, &index, &middle, &ring, &pinky);
        read_imu(&gx, &gy, &gz, &ax, &ay, &az, &qx, &qy, &qz, &qw);

        gesture_window[t][0] = gx;
        gesture_window[t][1] = gy;
        gesture_window[t][2] = gz;

        gesture_window[t][3] = ax;
        gesture_window[t][4] = ay;
        gesture_window[t][5] = az;

        gesture_window[t][6] = qx;
        gesture_window[t][7] = qy;
        gesture_window[t][8] = qz;
        gesture_window[t][9] = qw;

        gesture_window[t][10] = (float)pinky;
        gesture_window[t][11] = (float)ring;
        gesture_window[t][12] = (float)middle;
        gesture_window[t][13] = (float)index;
        gesture_window[t][14] = (float)thumb;

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
            "%d,%s,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
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
            gesture_window[t][13],
            gesture_window[t][14]
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