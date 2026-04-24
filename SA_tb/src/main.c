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
#define PI_VAL             3.14159265358979323846f
#define AMPLITUDE          0.25f  // safe level for 3W speaker, flat response zone 1k-10kHz

static i2s_chan_handle_t tx_handle = NULL;
static int16_t tone_buffer[TONE_SAMPLES * 2];

/* Musical note frequencies (Hz) — chosen in 1kHz-4kHz flat response band */
#define NOTE_C5   523.25f
#define NOTE_D5   587.33f
#define NOTE_E5   659.25f
#define NOTE_F5   698.46f
#define NOTE_G5   783.99f
#define NOTE_A5   880.00f
#define NOTE_B5   987.77f
#define NOTE_C6  1046.50f
#define NOTE_D6  1174.66f
#define NOTE_E6  1318.51f
#define NOTE_G6  1567.98f

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

static void build_sine_buffer(float freq_hz)
{
    for (int i = 0; i < TONE_SAMPLES; i++) {
        float t = (float)i / SAMPLE_RATE;
        float s = sinf(2.0f * PI_VAL * freq_hz * t);
        int16_t sample = (int16_t)(AMPLITUDE * 32767.0f * s);
        tone_buffer[2 * i]     = sample;
        tone_buffer[2 * i + 1] = sample;
    }
}

static void play_tone(float freq_hz, int duration_ms)
{
    build_sine_buffer(freq_hz);
    size_t bytes_written = 0;
    int chunks = (duration_ms / 10);
    if (chunks < 1) chunks = 1;
    for (int i = 0; i < chunks; i++) {
        i2s_channel_write(tx_handle, tone_buffer, sizeof(tone_buffer), &bytes_written, portMAX_DELAY);
    }
}

static void play_silence(int duration_ms)
{
    int16_t silence[TONE_SAMPLES * 2];
    memset(silence, 0, sizeof(silence));
    size_t bytes_written = 0;
    int chunks = (duration_ms / 10);
    if (chunks < 1) chunks = 1;
    for (int i = 0; i < chunks; i++) {
        i2s_channel_write(tx_handle, silence, sizeof(silence), &bytes_written, portMAX_DELAY);
    }
}

/* ── 12 Gesture-Sound Functions ──────────────────────────────────────────── */

/* HI — two rising cheerful notes (short-long) */
static void say_hi(void)
{
    printf("[say_hi]\n");
    play_tone(NOTE_G5, 150);
    play_silence(60);
    play_tone(NOTE_C6, 350);
    play_silence(300);
}

/* ONE — single solid long beep */
static void say_one(void)
{
    printf("[say_one]\n");
    play_tone(NOTE_C5, 600);
    play_silence(300);
}

/* TWO — two equal medium beeps */
static void say_two(void)
{
    printf("[say_two]\n");
    play_tone(NOTE_D5, 250);
    play_silence(100);
    play_tone(NOTE_D5, 250);
    play_silence(300);
}

/* THREE — three quick ascending beeps */
static void say_three(void)
{
    printf("[say_three]\n");
    play_tone(NOTE_E5, 180);
    play_silence(80);
    play_tone(NOTE_G5, 180);
    play_silence(80);
    play_tone(NOTE_B5, 180);
    play_silence(300);
}

/* FOUR — four short equal pulses */
static void say_four(void)
{
    printf("[say_four]\n");
    float notes[] = { NOTE_F5, NOTE_F5, NOTE_F5, NOTE_F5 };
    for (int i = 0; i < 4; i++) {
        play_tone(notes[i], 150);
        play_silence(80);
    }
    play_silence(220);
}

/* FIVE — ascending pentatonic scale (5 notes) */
static void say_five(void)
{
    printf("[say_five]\n");
    float scale[] = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5, NOTE_A5 };
    for (int i = 0; i < 5; i++) {
        play_tone(scale[i], 180);
        play_silence(60);
    }
    play_silence(200);
}

/* SIX — three pairs of staccato beeps (high-low rhythm) */
static void say_six(void)
{
    printf("[say_six]\n");
    for (int i = 0; i < 3; i++) {
        play_tone(NOTE_B5, 120);
        play_silence(60);
        play_tone(NOTE_G5, 120);
        play_silence(120);
    }
    play_silence(200);
}

/* SEVEN — rising arpeggio + accent high note */
static void say_seven(void)
{
    printf("[say_seven]\n");
    float arp[] = { NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6 };
    for (int i = 0; i < 4; i++) {
        play_tone(arp[i], 150);
        play_silence(50);
    }
    play_silence(80);
    play_tone(NOTE_E6, 300);
    play_silence(300);
}

/* EIGHT — four pairs, each pair ascending an octave */
static void say_eight(void)
{
    printf("[say_eight]\n");
    float lo[] = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5 };
    float hi[] = { NOTE_C6, NOTE_D6, NOTE_E6, NOTE_G6 };
    for (int i = 0; i < 4; i++) {
        play_tone(lo[i], 120);
        play_silence(40);
        play_tone(hi[i], 120);
        play_silence(100);
    }
    play_silence(200);
}

/* NINE — four ascending notes + single high flourish */
static void say_nine(void)
{
    printf("[say_nine]\n");
    float steps[] = { NOTE_D5, NOTE_F5, NOTE_A5, NOTE_D6 };
    for (int i = 0; i < 4; i++) {
        play_tone(steps[i], 160);
        play_silence(60);
    }
    play_tone(NOTE_G6, 400);
    play_silence(300);
}

/* TEN — two-note fanfare (one + zero: short-rest-long) */
static void say_ten(void)
{
    printf("[say_ten]\n");
    play_tone(NOTE_C6, 200);   // "one"
    play_silence(100);
    play_tone(NOTE_G5, 120);   // "ze-"
    play_silence(40);
    play_tone(NOTE_E5, 400);   // "-ro"
    play_silence(300);
}

/* BYE — two falling notes (long-short, descending farewell) */
static void say_bye(void)
{
    printf("[say_bye]\n");
    play_tone(NOTE_A5, 350);
    play_silence(80);
    play_tone(NOTE_E5, 200);
    play_silence(300);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

void app_main(void)
{
    printf("Gestura Speaker Testbench — 12 gesture sounds\n");
    ESP_ERROR_CHECK(audio_i2s_init());

    while (1) {
        say_hi();
        vTaskDelay(pdMS_TO_TICKS(500));

        say_one();
        say_two();
        say_three();
        say_four();
        say_five();
        say_six();
        say_seven();
        say_eight();
        say_nine();
        say_ten();

        vTaskDelay(pdMS_TO_TICKS(500));
        say_bye();

        printf("=== cycle complete ===\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
