#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define I2S_BCLK_GPIO  4
#define I2S_WS_GPIO    5
#define I2S_DOUT_GPIO  6

#define SAMPLE_RATE        48000
#define TONE_SAMPLES       256
#define PI_VAL             3.14159265358979323846

static i2s_chan_handle_t tx_handle = NULL;

/* 16-bit mono tone buffer, but sent as stereo frames
 * because standard I2S commonly uses L/R slots.
 * We duplicate the same sample into both channels.
 */
static int16_t tone_buffer[TONE_SAMPLES * 2];

static esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    return ESP_OK;
}

static void build_sine_buffer(float freq_hz, float amplitude)
{
    if (amplitude > 1.0f) amplitude = 1.0f;
    if (amplitude < 0.0f) amplitude = 0.0f;

    for (int i = 0; i < TONE_SAMPLES; i++) {
        float t = (float)i / SAMPLE_RATE;
        float s = sinf(2.0f * PI_VAL * freq_hz * t);
        int16_t sample = (int16_t)(amplitude * 32767.0f * s);

        // stereo frame: L then R
        tone_buffer[2 * i]     = sample;
        tone_buffer[2 * i + 1] = sample;
    }
}

static void play_tone(float freq_hz, float amplitude, int duration_ms)
{
    build_sine_buffer(freq_hz, amplitude);

    size_t bytes_written = 0;
    int total_chunks = duration_ms / 10;
    if (total_chunks < 1) total_chunks = 1;

    printf("Playing %.1f Hz, amplitude %.2f, duration %d ms\n",
           freq_hz, amplitude, duration_ms);

    for (int i = 0; i < total_chunks; i++) {
        ESP_ERROR_CHECK(
            i2s_channel_write(tx_handle,
                              tone_buffer,
                              sizeof(tone_buffer),
                              &bytes_written,
                              portMAX_DELAY)
        );
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}

static void play_silence(int duration_ms)
{
    int16_t silence[TONE_SAMPLES * 2];
    memset(silence, 0, sizeof(silence));

    size_t bytes_written = 0;
    int total_chunks = duration_ms / 10;
    if (total_chunks < 1) total_chunks = 1;

    printf("Silence for %d ms\n", duration_ms);

    for (int i = 0; i < total_chunks; i++) {
        ESP_ERROR_CHECK(
            i2s_channel_write(tx_handle,
                              silence,
                              sizeof(silence),
                              &bytes_written,
                              portMAX_DELAY)
        );
    }
}

static void fixed_tone_test(void)
{
    printf("\n=== Fixed Tone Test ===\n");
    play_tone(500.0f, 0.25f, 1500);
    play_silence(300);

    play_tone(1000.0f, 0.25f, 1500);
    play_silence(300);

    play_tone(2000.0f, 0.25f, 1500);
    play_silence(300);
}

static void amplitude_sweep_test(void)
{
    float amps[] = {0.10f, 0.20f, 0.35f, 0.50f, 0.70f};
    int n = sizeof(amps) / sizeof(amps[0]);

    printf("\n=== Amplitude Sweep Test @ 1 kHz ===\n");
    for (int i = 0; i < n; i++) {
        play_tone(1000.0f, amps[i], 1200);
        play_silence(250);
    }
}

static void frequency_sweep_test(void)
{
    float freqs[] = {100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};
    int n = sizeof(freqs) / sizeof(freqs[0]);

    printf("\n=== Frequency Sweep Test ===\n");
    for (int i = 0; i < n; i++) {
        play_tone(freqs[i], 0.25f, 1000);
        play_silence(200);
    }
}

void app_main(void)
{
    printf("MAX98357 + ESP32 I2S speaker/amplifier testbench start\n");

    ESP_ERROR_CHECK(audio_i2s_init());

    while (1) {
        fixed_tone_test();
        amplitude_sweep_test();
        frequency_sweep_test();

        printf("\n=== Test cycle complete ===\n");
        play_silence(2000);
    }
}