// SPDX-License-Identifier: MIT
#pragma once
#include "game.h"

#define GAME_PREFERENCES_BYTES 4u
/* Stable versioned record, separate from the journey; failed decode is atomic. */
bool game_preferences_encode(const game_preferences_t *,uint8_t *bytes);
bool game_preferences_decode(const uint8_t *bytes,game_preferences_t *);
