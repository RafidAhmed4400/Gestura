#ifndef GESTURE_MODEL_H
#define GESTURE_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define GM_NUM_TIMESTEPS 140
#define GM_NUM_FEATURES 15

// Change this to match the number of classes
#define GM_NUM_CLASSES 27

void gesture_model_init(void);

int gesture_model_predict(float input[GM_NUM_TIMESTEPS][GM_NUM_FEATURES]);

const char *gesture_model_get_class_name(int class_index);

#ifdef __cplusplus
}
#endif

#endif