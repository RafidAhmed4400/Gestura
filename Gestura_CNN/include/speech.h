#pragma once
// =============================================================================
// speech.h — Gestura speech playback API
// =============================================================================
// Include this wherever you need to play audio. Then call:
//
//   speech_play("letter_a");
//   speech_play("i_love_you");
//   speech_play_vol("fuck_you", 0.20f);
//
// To add new words/phrases: edit generate_audio.sh and re-run it.
// No changes needed in this file or main.c.
// =============================================================================

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "audio_all.h"          // auto-generated clip data + lookup table
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Default volume (0.0 = silent, 1.0 = full — stay ≤ 0.20 to avoid clipping)
#ifndef SPEECH_VOLUME
#define SPEECH_VOLUME  0.15f
#endif

#define SPEECH_CHUNK_FRAMES  256

// ── Internal: I2S handle must be set before calling speech_play ──────────────
static i2s_chan_handle_t *_speech_i2s_handle = (void*)0;

static inline void speech_init(i2s_chan_handle_t *handle) {
    _speech_i2s_handle = handle;
}

// ── Play raw samples at a given volume ───────────────────────────────────────
static inline void speech_play_raw(const int16_t *samples, size_t len, float vol) {
    if (!_speech_i2s_handle) {
        printf("[speech] ERROR: call speech_init() first\n");
        return;
    }
    int16_t buf[SPEECH_CHUNK_FRAMES * 2];
    size_t pos = 0;
    while (pos < len) {
        size_t frames = len - pos;
        if (frames > SPEECH_CHUNK_FRAMES) frames = SPEECH_CHUNK_FRAMES;
        for (size_t i = 0; i < frames; i++) {
            int16_t s = (int16_t)((float)samples[pos + i] * vol);
            buf[2 * i]     = s;
            buf[2 * i + 1] = s;
        }
        size_t written = 0;
        i2s_channel_write(*_speech_i2s_handle, buf,
                          frames * 2 * sizeof(int16_t),
                          &written, portMAX_DELAY);
        pos += frames;
    }
}

// ── Play a clip by name at custom volume ─────────────────────────────────────
static inline void speech_play_vol(const char *name, float vol) {
    const SpeechClip *clip = speech_find(name);
    if (!clip) {
        printf("[speech] '%s' not found\n", name);
        return;
    }
    printf("[speech] playing '%s'\n", name);
    speech_play_raw(clip->samples, clip->len, vol);
}

// ── Play a clip by name at default volume ────────────────────────────────────
static inline void speech_play(const char *name) {
    speech_play_vol(name, SPEECH_VOLUME);
}

// ── Check if a clip exists ───────────────────────────────────────────────────
static inline int speech_exists(const char *name) {
    return speech_find(name) != (void*)0;
}
