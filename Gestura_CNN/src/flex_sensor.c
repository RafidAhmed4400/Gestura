#include "flex_sensor.h"

const char *FSR_TAG = "FSR_APP";

static int clamp(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

esp_err_t flex_init(void) {
    esp_err_t ret;

    // ADC configuration
    ESP_RETURN_ON_ERROR( adc1_config_width(
        ADC_WIDTH_BIT_12), // 12-bit width, standard for ESP32
        "Failed to configure ADC1 width" 
    );

    // define configuration for attenuation
    ESP_RETURN_ON_ERROR( 
        adc1_config_channel_atten(FSR_ADC_CHANNEL_0, ADC_ATTEN_DB_11),
        "Failed to configure FSR0" 
    );
    ESP_RETURN_ON_ERROR( 
        adc1_config_channel_atten(FSR_ADC_CHANNEL_1, ADC_ATTEN_DB_11),
        "Failed to configure FSR1" 
    );
    ESP_RETURN_ON_ERROR( 
        adc1_config_channel_atten(FSR_ADC_CHANNEL_2, ADC_ATTEN_DB_11),
        "Failed to configure FSR2" 
    );
    ESP_RETURN_ON_ERROR( 
        adc1_config_channel_atten(FSR_ADC_CHANNEL_3, ADC_ATTEN_DB_11),
        "Failed to configure FSR3" 
    );
    ESP_RETURN_ON_ERROR( 
        adc1_config_channel_atten(FSR_ADC_CHANNEL_4, ADC_ATTEN_DB_11),
        "Failed to configure FSR4" 
    );
    ESP_RETURN_ON_ERROR( 
        adc2_config_channel_atten(FSR_ADC_CHANNEL_5, ADC_ATTEN_DB_11),
        "Failed to configure FSR5" 
    );
    ESP_RETURN_ON_ERROR( 
        adc2_config_channel_atten(FSR_ADC_CHANNEL_6, ADC_ATTEN_DB_11),
        "Failed to configure FSR6" 
    );
    ESP_RETURN_ON_ERROR( 
        adc2_config_channel_atten(FSR_ADC_CHANNEL_7, ADC_ATTEN_DB_11),
        "Failed to configure FSR7" 
    );
    ESP_RETURN_ON_ERROR( 
        adc2_config_channel_atten(FSR_ADC_CHANNEL_8, ADC_ATTEN_DB_11),
        "Failed to configure FSR8" 
    );
    ESP_RETURN_ON_ERROR( 
        adc2_config_channel_atten(FSR_ADC_CHANNEL_9, ADC_ATTEN_DB_11),
        "Failed to configure FSR9" 
    );

    return ESP_OK;
}

esp_err_t flex_read_normalized(flex_data_t *flex_data) {
    int fsr_thumb_raw_1 = adc1_get_raw(FSR_ADC_CHANNEL_0); 
    int fsr_thumb_raw_2 = adc1_get_raw(FSR_ADC_CHANNEL_1);
    int fsr_index_raw_1 = adc1_get_raw(FSR_ADC_CHANNEL_2);
    int fsr_index_raw_2 = adc1_get_raw(FSR_ADC_CHANNEL_3);
    int fsr_middle_raw_1 = adc1_get_raw(FSR_ADC_CHANNEL_4);
    int fsr_middle_raw_2 = adc1_get_raw(FSR_ADC_CHANNEL_5);
    int fsr_ring_raw_1 = adc1_get_raw(FSR_ADC_CHANNEL_6);
    int fsr_ring_raw_2 = adc1_get_raw(FSR_ADC_CHANNEL_7);
    int fsr_pinky_raw_1 = adc1_get_raw(FSR_ADC_CHANNEL_8);
    int fsr_pinky_raw_2 = adc1_get_raw(FSR_ADC_CHANNEL_9);

    // change the minmax values in flex_sensor.h to the correct values for the FSRs
    ESP_LOGI(FSR_TAG, "CHANGE THE MINMAX VALUES THEY ARE NOT CORRECT FOR THE FSRs");
    flex_data->fsr_thumb_1 = clamp((fsr_thumb_raw_1 - FSR_THUMB_MIN) * 255 / (FSR_THUMB_MAX - FSR_THUMB_MIN), 0, 255);
    flex_data->fsr_thumb_2 = clamp((fsr_thumb_raw_2 - FSR_THUMB_MIN) * 255 / (FSR_THUMB_MAX - FSR_THUMB_MIN), 0, 255);
    flex_data->fsr_index_1 = clamp((fsr_index_raw_1 - FSR_INDEX_MIN) * 255 / (FSR_INDEX_MAX - FSR_INDEX_MIN), 0, 255);
    flex_data->fsr_index_2 = clamp((fsr_index_raw_2 - FSR_INDEX_MIN) * 255 / (FSR_INDEX_MAX - FSR_INDEX_MIN), 0, 255);
    flex_data->fsr_middle_1 = clamp((fsr_middle_raw_1 - FSR_MIDDLE_MIN) * 255 / (FSR_MIDDLE_MAX - FSR_MIDDLE_MIN), 0, 255);
    flex_data->fsr_middle_2 = clamp((fsr_middle_raw_2 - FSR_MIDDLE_MIN) * 255 / (FSR_MIDDLE_MAX - FSR_MIDDLE_MIN), 0, 255);
    flex_data->fsr_ring_1 = clamp((fsr_ring_raw_1 - FSR_RING_MIN) * 255 / (FSR_RING_MAX - FSR_RING_MIN), 0, 255);
    flex_data->fsr_ring_2 = clamp((fsr_ring_raw_2 - FSR_RING_MIN) * 255 / (FSR_RING_MAX - FSR_RING_MIN), 0, 255);
    flex_data->fsr_pinky_1 = clamp((fsr_pinky_raw_1 - FSR_PINKY_MIN) * 255 / (FSR_PINKY_MAX - FSR_PINKY_MIN), 0, 255);
    flex_data->fsr_pinky_2 = clamp((fsr_pinky_raw_2 - FSR_PINKY_MIN) * 255 / (FSR_PINKY_MAX - FSR_PINKY_MIN), 0, 255);
    return ESP_OK;
}
