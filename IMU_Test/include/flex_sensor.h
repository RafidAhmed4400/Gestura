#ifndef _FLEX_SENSOR_H_
#define _FLEX_SENSOR_H_

#pragma once

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_LOG_TIMESTAMP_DISABLED 1
#include "esp_log.h"
#include "esp_err.h"  
#include "driver/adc.h" // ADC
#include "esp_timer.h"

extern const char *FSR_TAG;

// GPIO pin definitions and channel mappings for ADC and LEDC
#define FSR_ADC_CHANNEL_0 ADC1_CHANNEL_0   // GPIO1 pin1
#define FSR_ADC_CHANNEL_1 ADC1_CHANNEL_9   // GPIO2 pin10
#define FSR_ADC_CHANNEL_2 ADC1_CHANNEL_6   // GPIO4 pin3 
#define FSR_ADC_CHANNEL_3 ADC1_CHANNEL_7   // GPIO5 pin8
#define FSR_ADC_CHANNEL_4 ADC1_CHANNEL_2   // GPIO5 pin7

#define FSR_PINKY_MAX 598
#define FSR_PINKY_MIN 148
#define FSR_RING_MAX 543
#define FSR_RING_MIN 149
#define FSR_MIDDLE_MAX 570
#define FSR_MIDDLE_MIN 155
#define FSR_INDEX_MAX 582
#define FSR_INDEX_MIN 162
#define FSR_THUMB_MAX 663
#define FSR_THUMB_MIN 353

typedef struct flex_data_t {
    int fsr_pinky;
    int fsr_ring;
    int fsr_middle;
    int fsr_index;
    int fsr_thumb;
} flex_data_t;

// function def
esp_err_t flex_init(void);

esp_err_t flex_read(flex_data_t *flex_data);

esp_err_t flex_read_normalized(flex_data_t *flex_data);

#endif