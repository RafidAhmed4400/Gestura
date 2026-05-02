// Input: 140 x 15

// Conv1D layer 1:
//   filters = 8
//   kernel size = 5
//   activation = ReLU

// MaxPool1D:
//   pool size = 2

// Conv1D layer 2:
//   filters = 16
//   kernel size = 3
//   activation = ReLU

// Global average pooling

// Dense layer:
//   16 neurons
//   activation = ReLU

// Output layer:
//   GM_NUM_CLASSES neurons
//   softmax

#include "gesture_model.h"

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "GESTURE_MODEL";

// ------------------------------------------------------
// Model dimensions
// ------------------------------------------------------

#define CONV1_FILTERS 8
#define CONV1_KERNEL 5

#define POOL1_SIZE 2
#define POOL1_TIMESTEPS (GM_NUM_TIMESTEPS / POOL1_SIZE)

#define CONV2_FILTERS 16
#define CONV2_KERNEL 3

#define DENSE1_UNITS 16

// ------------------------------------------------------
// Class names
//
// Replace these with your actual gesture names.
// If you have 27 classes, update GM_NUM_CLASSES in gesture_model.h
// and add all names here.
// ------------------------------------------------------

static const char *class_names[GM_NUM_CLASSES] = {
    "A","B","C","D","E",
    "F","G","H","I","J",
    "K","L","M","N","O",
    "P","Q","R","S","T",
    "U","V","W","X","Y",
    "Z", "NO_GESTURE"
};


// Scaler parameters
// These must match your Python StandardScaler.
//
// In Python, after training:
//
// print(scaler.mean_)
// print(scaler.scale_)
//
// Then paste those values here.
//
// Feature order:
// gx, gy, gz, ax, ay, az, qx, qy, qz, qw, pinky, ring, middle, index, thumb


static const float scaler_mean[15] = {
    gx_mean, gy_mean, gz_mean,
    ax_mean, ay_mean, az_mean,
    qx_mean, qy_mean, qz_mean, qw_mean,
    pinky_mean, ring_mean, middle_mean, index_mean, thumb_mean
};

static const float scaler_std[15] = {
    gx_std, gy_std, gz_std,
    ax_std, ay_std, az_std,
    qx_std, qy_std, qz_std, qw_std,
    pinky_std, ring_std, middle_std, index_std, thumb_std
};

// ------------------------------------------------------
// CNN weights
//
// IMPORTANT:
// These are placeholder values.
// The code structure is correct, but these weights are not trained.
//
// After training in Python, export your model weights and paste them here.
// ------------------------------------------------------

// Conv1 weights shape:
// [CONV1_FILTERS][CONV1_KERNEL][GM_NUM_FEATURES]
static const float conv1_w[CONV1_FILTERS][CONV1_KERNEL][GM_NUM_FEATURES] = {
    {{{0}}}
};

// Conv1 bias shape:
// [CONV1_FILTERS]
static const float conv1_b[CONV1_FILTERS] = {
    0
};

// Conv2 weights shape:
// [CONV2_FILTERS][CONV2_KERNEL][CONV1_FILTERS]
static const float conv2_w[CONV2_FILTERS][CONV2_KERNEL][CONV1_FILTERS] = {
    {{{0}}}
};

// Conv2 bias shape:
// [CONV2_FILTERS]
static const float conv2_b[CONV2_FILTERS] = {
    0
};

// Dense1 weights shape:
// [DENSE1_UNITS][CONV2_FILTERS]
static const float dense1_w[DENSE1_UNITS][CONV2_FILTERS] = {
    {0}
};

// Dense1 bias shape:
// [DENSE1_UNITS]
static const float dense1_b[DENSE1_UNITS] = {
    0
};

// Output weights shape:
// [GM_NUM_CLASSES][DENSE1_UNITS]
static const float output_w[GM_NUM_CLASSES][DENSE1_UNITS] = {
    {0}
};

// Output bias shape:
// [GM_NUM_CLASSES]
static const float output_b[GM_NUM_CLASSES] = {
    0
};


// ------------------------------------------------------
// Internal buffers
// ------------------------------------------------------

static float x_scaled[GM_NUM_TIMESTEPS][GM_NUM_FEATURES];

static float conv1_out[GM_NUM_TIMESTEPS][CONV1_FILTERS];

static float pool1_out[POOL1_TIMESTEPS][CONV1_FILTERS];

static float conv2_out[POOL1_TIMESTEPS][CONV2_FILTERS];

static float gap_out[CONV2_FILTERS];

static float dense1_out[DENSE1_UNITS];

static float logits[GM_NUM_CLASSES];

static float probs[GM_NUM_CLASSES];


// ------------------------------------------------------
// Utility functions
// ------------------------------------------------------

static float relu(float x)
{
    return x > 0.0f ? x : 0.0f;
}


static void softmax(const float *input, float *output, int length)
{
    float max_val = input[0];

    for (int i = 1; i < length; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    float sum = 0.0f;

    for (int i = 0; i < length; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    if (sum <= 0.0f) {
        return;
    }

    for (int i = 0; i < length; i++) {
        output[i] /= sum;
    }
}


static int argmax(const float *array, int length)
{
    int best_index = 0;
    float best_value = array[0];

    for (int i = 1; i < length; i++) {
        if (array[i] > best_value) {
            best_value = array[i];
            best_index = i;
        }
    }

    return best_index;
}


// ------------------------------------------------------
// Normalize input
//
// Same idea as Python StandardScaler:
//
// x_scaled = (x - mean) / std
// ------------------------------------------------------

static void normalize_input(float input[GM_NUM_TIMESTEPS][GM_NUM_FEATURES])
{
    for (int t = 0; t < GM_NUM_TIMESTEPS; t++) {
        for (int f = 0; f < GM_NUM_FEATURES; f++) {
            float std = scaler_std[f];

            if (std == 0.0f) {
                std = 1.0f;
            }

            x_scaled[t][f] = (input[t][f] - scaler_mean[f]) / std;
        }
    }
}


// ------------------------------------------------------
// Conv1D layer 1
//
// padding = same
// activation = ReLU
// ------------------------------------------------------

static void conv1d_layer1(void)
{
    int pad = CONV1_KERNEL / 2;

    for (int t = 0; t < GM_NUM_TIMESTEPS; t++) {
        for (int filter = 0; filter < CONV1_FILTERS; filter++) {
            float sum = conv1_b[filter];

            for (int k = 0; k < CONV1_KERNEL; k++) {
                int input_t = t + k - pad;

                if (input_t < 0 || input_t >= GM_NUM_TIMESTEPS) {
                    continue;
                }

                for (int f = 0; f < GM_NUM_FEATURES; f++) {
                    sum += x_scaled[input_t][f] * conv1_w[filter][k][f];
                }
            }

            conv1_out[t][filter] = relu(sum);
        }
    }
}


// ------------------------------------------------------
// MaxPool1D layer
//
// pool size = 2
// ------------------------------------------------------

static void maxpool1d_layer1(void)
{
    for (int t = 0; t < POOL1_TIMESTEPS; t++) {
        int t0 = t * POOL1_SIZE;
        int t1 = t0 + 1;

        for (int f = 0; f < CONV1_FILTERS; f++) {
            float max_val = conv1_out[t0][f];

            if (t1 < GM_NUM_TIMESTEPS && conv1_out[t1][f] > max_val) {
                max_val = conv1_out[t1][f];
            }

            pool1_out[t][f] = max_val;
        }
    }
}


// ------------------------------------------------------
// Conv1D layer 2
//
// input shape:
//   POOL1_TIMESTEPS x CONV1_FILTERS
//
// output shape:
//   POOL1_TIMESTEPS x CONV2_FILTERS
//
// padding = same
// activation = ReLU
// ------------------------------------------------------

static void conv1d_layer2(void)
{
    int pad = CONV2_KERNEL / 2;

    for (int t = 0; t < POOL1_TIMESTEPS; t++) {
        for (int filter = 0; filter < CONV2_FILTERS; filter++) {
            float sum = conv2_b[filter];

            for (int k = 0; k < CONV2_KERNEL; k++) {
                int input_t = t + k - pad;

                if (input_t < 0 || input_t >= POOL1_TIMESTEPS) {
                    continue;
                }

                for (int f = 0; f < CONV1_FILTERS; f++) {
                    sum += pool1_out[input_t][f] * conv2_w[filter][k][f];
                }
            }

            conv2_out[t][filter] = relu(sum);
        }
    }
}


// ------------------------------------------------------
// Global Average Pooling
//
// Converts:
//   POOL1_TIMESTEPS x CONV2_FILTERS
//
// Into:
//   CONV2_FILTERS
// ------------------------------------------------------

static void global_average_pooling(void)
{
    for (int f = 0; f < CONV2_FILTERS; f++) {
        float sum = 0.0f;

        for (int t = 0; t < POOL1_TIMESTEPS; t++) {
            sum += conv2_out[t][f];
        }

        gap_out[f] = sum / (float)POOL1_TIMESTEPS;
    }
}


// ------------------------------------------------------
// Dense layer 1
//
// activation = ReLU
// ------------------------------------------------------

static void dense_layer1(void)
{
    for (int unit = 0; unit < DENSE1_UNITS; unit++) {
        float sum = dense1_b[unit];

        for (int f = 0; f < CONV2_FILTERS; f++) {
            sum += gap_out[f] * dense1_w[unit][f];
        }

        dense1_out[unit] = relu(sum);
    }
}


// ------------------------------------------------------
// Output layer
//
// activation = softmax
// ------------------------------------------------------

static void output_layer(void)
{
    for (int c = 0; c < GM_NUM_CLASSES; c++) {
        float sum = output_b[c];

        for (int unit = 0; unit < DENSE1_UNITS; unit++) {
            sum += dense1_out[unit] * output_w[c][unit];
        }

        logits[c] = sum;
    }

    softmax(logits, probs, GM_NUM_CLASSES);
}


// ------------------------------------------------------
// Public functions
// ------------------------------------------------------

void gesture_model_init(void)
{
    ESP_LOGI(TAG, "Gesture model initialized.");
    ESP_LOGI(TAG, "Input shape: %d x %d", GM_NUM_TIMESTEPS, GM_NUM_FEATURES);
    ESP_LOGI(TAG, "Number of classes: %d", GM_NUM_CLASSES);
}


int gesture_model_predict(float input[GM_NUM_TIMESTEPS][GM_NUM_FEATURES])
{
    normalize_input(input);

    conv1d_layer1();

    maxpool1d_layer1();

    conv1d_layer2();

    global_average_pooling();

    dense_layer1();

    output_layer();

    int predicted_class = argmax(probs, GM_NUM_CLASSES);

    ESP_LOGI(TAG, "Class probabilities:");

    for (int i = 0; i < GM_NUM_CLASSES; i++) {
        ESP_LOGI(TAG, "  %s: %.4f", class_names[i], probs[i]);
    }

    return predicted_class;
}


const char *gesture_model_get_class_name(int class_index)
{
    if (class_index < 0 || class_index >= GM_NUM_CLASSES) {
        return "UNKNOWN";
    }

    return class_names[class_index];
}