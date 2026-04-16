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
#define FSR_ADC_CHANNEL_1 ADC1_CHANNEL_1   // GPIO2 pin2
#define FSR_ADC_CHANNEL_2 ADC1_CHANNEL_2   // GPIO4 pin3 
#define FSR_ADC_CHANNEL_3 ADC1_CHANNEL_6   // GPIO5 pin7
#define FSR_ADC_CHANNEL_4 ADC1_CHANNEL_7   // GPIO5 pin8
#define LED_GPIO 2 // GPIO pin for LED output

void app_main(void)
{
    // ADC configuration
    adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit width, standard for ESP32

    // define configuration for attenuation
    adc1_config_channel_atten(FSR_ADC_CHANNEL_1, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_0, ADC_ATTEN_DB_11); 
    adc1_config_channel_atten(FSR_ADC_CHANNEL_2, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(FSR_ADC_CHANNEL_4, ADC_ATTEN_DB_11);

    // PWM configuration using LEDC
    ledc_timer_config_t ledc_timer = { 
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT, 
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LED_GPIO,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    // Main loop to read ADC values, map them to PWM duty cycle, and update the LED brightness
    while (1) {
        // Read flex sensor value from ADC
        int fsr_pinky = adc1_get_raw(FSR_ADC_CHANNEL_0);
        int fsr_ring = adc1_get_raw(FSR_ADC_CHANNEL_1);
        int fsr_middle = adc1_get_raw(FSR_ADC_CHANNEL_2);
        int fsr_index = adc1_get_raw(FSR_ADC_CHANNEL_3);
        int fsr_thumb = adc1_get_raw(FSR_ADC_CHANNEL_4);

        // log the raw ADC values for debugging purposes
        ESP_LOGI(TAG, "Analog reading = %d", fsr_pinky);
        ESP_LOGI(TAG, "Analog reading = %d", fsr_ring);
        ESP_LOGI(TAG, "Analog reading = %d", fsr_middle);
        ESP_LOGI(TAG, "Analog reading = %d", fsr_index);
        ESP_LOGI(TAG, "Analog reading = %d", fsr_thumb);

        // Map 0–4095 to 0–255
        int LEDbrightness = (fsr_pinky * 255) / 4095;

        // --- SET PWM DUTY ---
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDbrightness);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);


        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
