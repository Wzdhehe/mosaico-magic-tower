// SPDX-License-Identifier: MIT
#pragma once
#define GAME_NAME "magic_tower"
#define GAME_VERSION "v1.1"
/* Magic Tower is turn-based: halve the continuous draw/logic cadence.
 * Durations and held-key repetition are expressed relative to this rate. */
enum { GAME_WIDTH = 480, GAME_HEIGHT = 480, GAME_TICK_HZ = 15 };
enum {
    GAME_NOTICE_TICKS = 3 * GAME_TICK_HZ,
    GAME_GAIN_TICKS = 4 * GAME_TICK_HZ
};
enum {
    GAME_ACTION_LEFT = 0, GAME_ACTION_RIGHT = 1, GAME_ACTION_UP = 2,
    GAME_ACTION_PAUSE = 3, GAME_ACTION_RESET = 4, GAME_ACTION_DOWN = 5,
    GAME_ACTION_START = 6
};
enum {
    GAME_MAP_X = 64, GAME_MAP_Y = 76, GAME_TILE = 32,
    GAME_FOOTER_Y = 448, GAME_FOOTER_H = 28,
    GAME_BUTTON_W = 90, GAME_BUTTON_GAP = 15, GAME_BUTTON_X = 90,
    /* 4 ticks at 15 Hz is ~267 ms, close to 7 ticks at 30 Hz (~233 ms). */
    GAME_SWIPE_MIN = 18, GAME_KEY_REPEAT = 4,
    GAME_LANG_X = 396, GAME_LANG_Y = 14, GAME_LANG_W = 64, GAME_LANG_H = 40,
    GAME_VOL_X = 20, GAME_VOL_Y = 14, GAME_VOL_W = 64, GAME_VOL_H = 40,
    GAME_VOL_PANEL_X = 110, GAME_VOL_PANEL_Y = 160, GAME_VOL_PANEL_W = 260, GAME_VOL_PANEL_H = 150,
    GAME_VOL_SLIDER_X = 130, GAME_VOL_SLIDER_Y = 222, GAME_VOL_SLIDER_W = 220, GAME_VOL_SLIDER_H = 22,
    GAME_VOL_CLOSE_X = 180, GAME_VOL_CLOSE_Y = 270, GAME_VOL_CLOSE_W = 120, GAME_VOL_CLOSE_H = 32
};
