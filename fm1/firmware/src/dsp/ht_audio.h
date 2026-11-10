/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Main-loop game events; decoding belongs exclusively to the audio callback. */
void ht_audio_game_reset(void);
void ht_audio_speech_window(int open);
void ht_audio_update(uint32_t elapsed_ms, uint32_t game_keys, int paused,
                     int feeding_mode, int state, int served, int spilled,
                     int reloaded);
float ht_audio_mouth(void);

/* Read-only counters for host/device validation. No allocation or audio I/O. */
uint32_t ht_audio_debug(unsigned index);
