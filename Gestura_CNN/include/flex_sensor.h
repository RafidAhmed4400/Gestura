#ifndef _FLEX_SENSOR_H_
#define _FLEX_SENSOR_H_

#pragma once

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"  
#include "driver/adc.h" // ADC
#include "esp_timer.h"

extern const char *FSR_TAG;

// GPIO pin definitions and channel mappings for ADC and LEDC
#define FSR_ADC_CHANNEL_0 ADC2_CHANNEL_0   // GPIO11 pin11
#define FSR_ADC_CHANNEL_1 ADC2_CHANNEL_1   // GPIO12 pin12
#define FSR_ADC_CHANNEL_2 ADC1_CHANNEL_7   // GPIO8 pin8
#define FSR_ADC_CHANNEL_3 ADC1_CHANNEL_2   // GPIO3 pin3 
#define FSR_ADC_CHANNEL_4 ADC2_CHANNEL_5   // GPIO16 pin16
#define FSR_ADC_CHANNEL_5 ADC2_CHANNEL_6   // GPIO17 pin17
#define FSR_ADC_CHANNEL_6 ADC1_CHANNEL_6   // GPIO7 pin7
#define FSR_ADC_CHANNEL_7 ADC2_CHANNEL_4   // GPIO15 pin15
#define FSR_ADC_CHANNEL_8 ADC1_CHANNEL_1   // GPIO2 pin2
#define FSR_ADC_CHANNEL_9 ADC1_CHANNEL_0   // GPIO1 pin1

#define FSR_PINKY_MAX 520
#define FSR_PINKY_MIN 128
#define FSR_RING_MAX 513
#define FSR_RING_MIN 149
#define FSR_MIDDLE_MAX 540
#define FSR_MIDDLE_MIN 145
#define FSR_INDEX_MAX 562
#define FSR_INDEX_MIN 172
#define FSR_THUMB_MAX 667
#define FSR_THUMB_MIN 340

typedef struct flex_data_t {
    int fsr_thumb_1;
    int fsr_thumb_2;   
    int fsr_index_1;
    int fsr_index_2;
    int fsr_middle_1;
    int fsr_middle_2;
    int fsr_ring_1;
    int fsr_ring_2;
    int fsr_pinky_1;
    int fsr_pinky_2;



} flex_data_t;

// function def
esp_err_t flex_init(void);

esp_err_t flex_read(flex_data_t *flex_data);

esp_err_t flex_read_normalized(flex_data_t *flex_data);

#endif