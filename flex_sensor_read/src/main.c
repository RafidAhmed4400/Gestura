#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_LOG_TIMESTAMP_DISABLED 1
#include "esp_log.h"
#include "esp_err.h"  
#include "driver/adc.h" // ADC
#include "esp_timer.h"
static const char *TAG = "FSR_APP";

// GPIO pin definitions and channel mappings for ADC and LEDC
#define FSR_ADC_CHANNEL_0 ADC1_CHANNEL_0   // GPIO1 pin1
#define FSR_ADC_CHANNEL_1 ADC1_CHANNEL_9   // GPIO2 pin10
#define FSR_ADC_CHANNEL_2 ADC1_CHANNEL_6   // GPIO4 pin3 
#define FSR_ADC_CHANNEL_3 ADC1_CHANNEL_7   // GPIO5 pin8
#define FSR_ADC_CHANNEL_4 ADC1_CHANNEL_2   // GPIO5 pin7

static int clamp(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

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

    // header for CSV output
    printf("time_ms,pinky_norm,ring_norm,middle_norm,index_norm,thumb_norm\n");


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

        int64_t t_ms = esp_timer_get_time() / 1000;
        // Print the normalized values in CSV format
        printf("%lld,%d,%d,%d,%d,%d\n", t_ms, // time in milliseconds
            clamp(fsr_pinky_normalized, 0, 255), 
            clamp(fsr_ring_normalized, 0, 255), 
            clamp(fsr_middle_normalized, 0, 255), 
            clamp(fsr_index_normalized, 0, 255), 
            clamp(fsr_thumb_normalized, 0, 255));
        
        fflush(stdout); // Ensure the output is printed immediately


        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
