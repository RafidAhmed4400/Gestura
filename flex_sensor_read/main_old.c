
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_LOG_TIMESTAMP_DISABLED 1
#include "esp_log.h"
#include "esp_err.h"  
#include "driver/adc.h" // ADC
#include "driver/ledc.h" // PWM (LEDC)

static const char *TAG = "FSR_APP";

// GPIO pin definitions and channel mappings for ADC and LEDC
#define FSR_ADC_CHANNEL_0 ADC1_CHANNEL_0   // GPIO1 pin1
#define FSR_ADC_CHANNEL_1 ADC1_CHANNEL_9   // GPIO2 pin10
#define FSR_ADC_CHANNEL_2 ADC1_CHANNEL_6   // GPIO4 pin3 
#define FSR_ADC_CHANNEL_3 ADC1_CHANNEL_7   // GPIO5 pin8
#define FSR_ADC_CHANNEL_4 ADC1_CHANNEL_2   // GPIO5 pin7
#define LED_GPIO 2 // GPIO pin for LED output

void app_main(void)
{
    // ADC configuration
    adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit width, standard for ESP32

    // define configuration for attenuation
    adc1_config_channel_atten(FSR_ADC_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_1, ADC_ATTEN_DB_11); 
    adc1_config_channel_atten(FSR_ADC_CHANNEL_2, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_4, ADC_ATTEN_DB_11);

    // // PWM configuration using LEDC
    // ledc_timer_config_t ledc_timer = { 
    //     .speed_mode       = LEDC_LOW_SPEED_MODE,
    //     .timer_num        = LEDC_TIMER_0,
    //     .duty_resolution  = LEDC_TIMER_8_BIT, 
    //     .freq_hz          = 5000,
    //     .clk_cfg          = LEDC_AUTO_CLK
    // };
    // ledc_timer_config(&ledc_timer);
    // ledc_channel_config_t ledc_channel = {
    //     .gpio_num       = LED_GPIO,
    //     .speed_mode     = LEDC_LOW_SPEED_MODE,
    //     .channel        = LEDC_CHANNEL_0,
    //     .timer_sel      = LEDC_TIMER_0,
    //     .duty           = 0,
    //     .hpoint         = 0
    // };
    // ledc_channel_config(&ledc_channel);

    // Main loop to read ADC values, map them to PWM duty cycle, and update the LED brightness
    while (1) {
        // Read flex sensor value from ADC
        int fsr_pinky = adc1_get_raw(FSR_ADC_CHANNEL_0);
        int fsr_ring = adc1_get_raw(FSR_ADC_CHANNEL_1);
        int fsr_middle = adc1_get_raw(FSR_ADC_CHANNEL_2);
        int fsr_index = adc1_get_raw(FSR_ADC_CHANNEL_3);
        int fsr_thumb = adc1_get_raw(FSR_ADC_CHANNEL_4); 

        // Normalize each sensor reading to a 0-255 range
        // define the max and min of each sensor reading. 
        // pinky range = 598 ~ 168
        // ring range = 543 ~ 149
        // middle range = 570 ~ 155
        // index range = 582 ~ 162
        // thumb range = 663 ~ 363
        int fsr_pinky_normalized = (fsr_pinky - 168) * 255 / (598 - 168);
        int fsr_ring_normalized = (fsr_ring - 149) * 255 / (543 - 149);
        int fsr_middle_normalized = (fsr_middle - 155) * 255 / (570 - 155);
        int fsr_index_normalized = (fsr_index - 162) * 255 / (582 - 162);
        int fsr_thumb_normalized = (fsr_thumb - 363) * 255 / (663 - 363);

        // log the normalized values for debugging purposes
        ESP_LOGI(TAG, "Pinky Normalized reading = %d", fsr_pinky_normalized);
        ESP_LOGI(TAG, "Ring Normalized reading = %d", fsr_ring_normalized);
        ESP_LOGI(TAG, "Middle Normalized reading = %d", fsr_middle_normalized);
        ESP_LOGI(TAG, "Index Normalized reading = %d", fsr_index_normalized);
        ESP_LOGI(TAG, "Thumb Normalized reading = %d", fsr_thumb_normalized);

        // // Run a moving average filter and normalize to smooth out the readings
        // int N = 10; // Number of samples for moving average
        // int fsr_pinky_avg = fsr_pinky; // Placeholder for moving average calculation
        // int fsr_ring_avg = fsr_ring; // Placeholder for moving average calculation
        // int fsr_middle_avg = fsr_middle; // Placeholder for moving average calculation
        // int fsr_index_avg = fsr_index; // Placeholder for moving average calculation
        // int fsr_thumb_avg = fsr_thumb; // Placeholder for moving average calculation

        // int index = 0; // Index for buffer
        // int pinky_arr[N]; // Array to hold the last N readings for pinky
        // if (index < N) {
        //     pinky_arr[index] = fsr_pinky; // Add new reading to the array
        //     index = (index + 1); // Update index
        // }
        
        // fsr_pinky_avg = (fsr_pinky_avg * (N - 1) + fsr_pinky) / N;
        

        // // Map 0–4095 to 0–255
        // int LEDbrightness = (fsr_pinky * 255) / 4095;

        // // --- SET PWM DUTY ---
        // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDbrightness);
        // ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);


        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
