// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define GAME_MUSIC_RATE 24000U
#define GAME_MUSIC_TRACKS 8U
/* Each handle has one owning thread. Immutable score/wave tables are shared.
 * Create allocates once, render/setters never allocate; delete accepts NULL. */
typedef struct game_music_t *game_music_handle_t;
typedef struct { unsigned reserved; } game_music_config_t;
bool game_music_create(const game_music_config_t *config, game_music_handle_t *ret_handle);
void game_music_delete(game_music_handle_t music);
bool game_music_set_volume(game_music_handle_t music, unsigned volume);
bool game_music_set_track(game_music_handle_t music, unsigned track);
bool game_music_render(game_music_handle_t music, int16_t *output, size_t frames);
uint32_t game_music_loop_samples(unsigned track);
uint64_t game_music_position(game_music_handle_t music);
const char *game_music_track_name(unsigned track);
size_t game_music_memory_bytes(void);
