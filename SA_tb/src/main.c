#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"

// ── Auto-generated speech audio (run generate_audio.sh on your Mac first) ──
// If the file doesn't exist yet, run:  ./generate_audio.sh
#include "audio_data.h"

// ── I2S pin assignments ──────────────────────────────────────────────────────
#define I2S_BCLK_GPIO   4
#define I2S_WS_GPIO     5
#define I2S_DOUT_GPIO   6

// ── Audio config ─────────────────────────────────────────────────────────────
#define SAMPLE_RATE     48000
// Chunk size for streaming — 512 stereo frames = 2048 bytes per write call
#define CHUNK_FRAMES    512
// Volume: divide samples by this (1=full, 2=50%, 4=25%, 8=12.5%)
// Raise if too quiet, lower if still distorting
#define VOLUME_DIVISOR  8

static i2s_chan_handle_t tx_handle = NULL;

// ── I2S init ─────────────────────────────────────────────────────────────────
static esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    return ESP_OK;
}

// ── Play a mono PCM clip — duplicated to stereo on the fly ───────────────────
// The HIBOX3070-K is mono, but I2S is configured stereo (L+R same sample).
static void play_clip(const int16_t *samples, size_t num_samples)
{
    // Stereo interleave buffer: [L, R, L, R, ...]
    int16_t stereo[CHUNK_FRAMES * 2];

    size_t pos = 0;
    while (pos < num_samples) {
        size_t frames = num_samples - pos;
        if (frames > CHUNK_FRAMES) frames = CHUNK_FRAMES;

        for (size_t i = 0; i < frames; i++) {
            int16_t s = samples[pos + i] / VOLUME_DIVISOR;
            stereo[2 * i]     = s;   // Left
            stereo[2 * i + 1] = s;   // Right (same)
        }

        size_t bytes_written = 0;
        i2s_channel_write(tx_handle,
                          stereo,
                          frames * 2 * sizeof(int16_t),
                          &bytes_written,
                          portMAX_DELAY);
        pos += frames;
    }
}

// Convenience: play a named clip from the table + a short pause after
static void say(int clip_index, const char *label)
{
    printf("[say] %s\n", label);
    const AudioClip *c = &AUDIO_CLIPS[clip_index];
    play_clip(c->samples, c->len);
    // 300 ms silence between words
    vTaskDelay(pdMS_TO_TICKS(300));
}

// ── Clip index helpers (matches ORDER in generate_audio.sh) ─────────────────
#define CLIP_HI     0
#define CLIP_ONE    1
#define CLIP_TWO    2
#define CLIP_THREE  3
#define CLIP_FOUR   4
#define CLIP_FIVE   5
#define CLIP_SIX    6
#define CLIP_SEVEN  7
#define CLIP_EIGHT  8
#define CLIP_NINE   9
#define CLIP_TEN    10
#define CLIP_BYE    11

// ── Main ─────────────────────────────────────────────────────────────────────
void app_main(void)
{
    printf("Gestura SA_tb — spoken word audio test\n");
    printf("I2S: GPIO BCLK=%d  WS=%d  DOUT=%d  @ %d Hz\n",
           I2S_BCLK_GPIO, I2S_WS_GPIO, I2S_DOUT_GPIO, SAMPLE_RATE);
    printf("Speaker: HIBOX3070-K  4ohm 3W\n");

    ESP_ERROR_CHECK(audio_i2s_init());
    printf("✓ I2S ready\n\n");

    while (1) {
        printf("--- gesture sound cycle ---\n");

        say(CLIP_HI,    "hi");
        vTaskDelay(pdMS_TO_TICKS(500));

        say(CLIP_ONE,   "one");
        say(CLIP_TWO,   "two");
        say(CLIP_THREE, "three");
        say(CLIP_FOUR,  "four");
        say(CLIP_FIVE,  "five");
        say(CLIP_SIX,   "six");
        say(CLIP_SEVEN, "seven");
        say(CLIP_EIGHT, "eight");
        say(CLIP_NINE,  "nine");
        say(CLIP_TEN,   "ten");

        vTaskDelay(pdMS_TO_TICKS(500));
        say(CLIP_BYE,   "bye");

        printf("=== cycle complete, waiting 3s ===\n\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
