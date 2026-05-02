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
    ret = adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit width, standard for ESP32

    // define configuration for attenuation
    ret = adc1_config_channel_atten(FSR_ADC_CHANNEL_0, ADC_ATTEN_DB_11);
    ret = adc1_config_channel_atten(FSR_ADC_CHANNEL_1, ADC_ATTEN_DB_11); 
    ret = adc1_config_channel_atten(FSR_ADC_CHANNEL_2, ADC_ATTEN_DB_11);
    ret = adc1_config_channel_atten(FSR_ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    ret = adc1_config_channel_atten(FSR_ADC_CHANNEL_4, ADC_ATTEN_DB_11);

    return ret;
}

esp_err_t flex_read(flex_data_t *flex_data) {
    flex_data->fsr_pinky = adc1_get_raw(FSR_ADC_CHANNEL_0);
    flex_data->fsr_ring = adc1_get_raw(FSR_ADC_CHANNEL_1);
    flex_data->fsr_middle = adc1_get_raw(FSR_ADC_CHANNEL_2);
    flex_data->fsr_index = adc1_get_raw(FSR_ADC_CHANNEL_3);
    flex_data->fsr_thumb = adc1_get_raw(FSR_ADC_CHANNEL_4); 

    return ESP_OK;
}

esp_err_t flex_read_normalized(flex_data_t *flex_data) {

    int fsr_pinky_raw = adc1_get_raw(FSR_ADC_CHANNEL_0);
    int fsr_ring_raw = adc1_get_raw(FSR_ADC_CHANNEL_1);
    int fsr_middle_raw = adc1_get_raw(FSR_ADC_CHANNEL_2);
    int fsr_index_raw = adc1_get_raw(FSR_ADC_CHANNEL_3);
    int fsr_thumb_raw = adc1_get_raw(FSR_ADC_CHANNEL_4); 

    flex_data->fsr_pinky = clamp((fsr_pinky_raw - FSR_PINKY_MIN) * 255 / (FSR_PINKY_MAX - FSR_PINKY_MIN), 0, 255);
    flex_data->fsr_ring = clamp((fsr_ring_raw - FSR_RING_MIN) * 255 / (FSR_RING_MAX - FSR_RING_MIN), 0, 255);
    flex_data->fsr_middle = clamp((fsr_middle_raw - FSR_MIDDLE_MIN) * 255 / (FSR_MIDDLE_MAX - FSR_MIDDLE_MIN), 0, 255);
    flex_data->fsr_index = clamp((fsr_index_raw - FSR_INDEX_MIN) * 255 / (FSR_INDEX_MAX - FSR_INDEX_MIN), 0, 255);
    flex_data->fsr_thumb = clamp((fsr_thumb_raw - FSR_THUMB_MIN) * 255 / (FSR_THUMB_MAX - FSR_THUMB_MIN), 0, 255);

    return ESP_OK;
}
