#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "speech.h"   // ← all audio lives here

//I2S pin assignments
#define I2S_BCLK_GPIO   4
#define I2S_WS_GPIO     5
#define I2S_DOUT_GPIO   6
#define SAMPLE_RATE     48000
#define DMA_DESC_NUM    4
#define DMA_FRAME_NUM   128

static i2s_chan_handle_t tx_handle = NULL;

static esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
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

// Main function
void app_main(void)
{
    ESP_ERROR_CHECK(audio_i2s_init());
    speech_init(&tx_handle);   // connect speech API to I2S
    printf("Gestura SA_tb ready — %d clips loaded\n\n", SPEECH_CLIP_COUNT);

    while (1) {
        // Alphabet
        const char *letters[] = {
            "letter_a","letter_b","letter_c","letter_d","letter_e","letter_f",
            "letter_g","letter_h","letter_i","letter_j","letter_k","letter_l",
            "letter_m","letter_n","letter_o","letter_p","letter_q","letter_r",
            "letter_s","letter_t","letter_u","letter_v","letter_w","letter_x",
            "letter_y","letter_z"
        };
        for (int i = 0; i < 26; i++) {
            speech_play(letters[i]);
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

        // Phrases
        speech_play("i_love_you");
        vTaskDelay(pdMS_TO_TICKS(1500));

        speech_play_vol("fuck_you", 0.20f);   // slightly louder, still clean
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
