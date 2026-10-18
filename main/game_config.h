// SPDX-License-Identifier: MIT
#pragma once
#define GAME_NAME "magic_tower"
#define GAME_VERSION "v1.7"
/* Operation-driven rendering; this is the maximum active update cadence.
 * Held-key repetition is expressed relative to this rate. */
enum { GAME_WIDTH = 480, GAME_HEIGHT = 480, GAME_TICK_HZ = 15 };
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
    GAME_SETTINGS_X = 414, GAME_SETTINGS_Y = 14, GAME_SETTINGS_W = 50, GAME_SETTINGS_H = 44,
    /* Preserve the original language button's touch rectangle, including its
     * 6px margin, while the visible gear retains its own geometry. */
    GAME_SETTINGS_TOUCH_X = 390, GAME_SETTINGS_TOUCH_Y = 8,
    GAME_SETTINGS_TOUCH_W = 76, GAME_SETTINGS_TOUCH_H = 52,
    GAME_SETTINGS_PANEL_X = 56, GAME_SETTINGS_PANEL_Y = 66,
    GAME_SETTINGS_PANEL_W = 368, GAME_SETTINGS_PANEL_H = 372,
    GAME_SETTINGS_CLOSE_X = 372, GAME_SETTINGS_CLOSE_Y = 84,
    GAME_SETTINGS_CLOSE_W = 36, GAME_SETTINGS_CLOSE_H = 36,
    GAME_SETTINGS_ZH_X = 218, GAME_SETTINGS_ZH_Y = 140,
    GAME_SETTINGS_ZH_W = 76, GAME_SETTINGS_ZH_H = 36,
    GAME_SETTINGS_EN_X = 306, GAME_SETTINGS_EN_Y = 140,
    GAME_SETTINGS_EN_W = 94, GAME_SETTINGS_EN_H = 36,
    GAME_SETTINGS_VOLUME_X = 80, GAME_SETTINGS_VOLUME_Y = 224,
    GAME_SETTINGS_VOLUME_W = 320, GAME_SETTINGS_VOLUME_H = 18,
    GAME_SETTINGS_BRIGHTNESS_X = 80, GAME_SETTINGS_BRIGHTNESS_Y = 300,
    GAME_SETTINGS_BRIGHTNESS_W = 320, GAME_SETTINGS_BRIGHTNESS_H = 18,
    GAME_SETTINGS_SLIDER_MARGIN = 12,
    GAME_BRIGHTNESS_MIN = 10, GAME_BRIGHTNESS_MAX = 100, GAME_BRIGHTNESS_DEFAULT = 40,
    /* Camera pages at an entered edge with a three-tile overlap. No camera
     * touch controls overlay the map; the visible extension hint is paint only. */
    GAME_CAMERA_EDGE_OVERLAP = 3, GAME_CAMERA_EDGE_HINT = 8,
    /* Teleport panel: the six chapter anchors in a fixed 2x3 grid. There is
     * no paging because there is nothing else to list. */
    GAME_TELEPORT_PANEL_X = 48, GAME_TELEPORT_PANEL_Y = 96,
    GAME_TELEPORT_PANEL_W = 384, GAME_TELEPORT_PANEL_H = 292,
    GAME_TELEPORT_CELL_W = 150, GAME_TELEPORT_CELL_H = 56,
    GAME_TELEPORT_GAP_X = 24, GAME_TELEPORT_GAP_Y = 14,
    GAME_TELEPORT_CELL_X = 78, GAME_TELEPORT_CELL_Y = 152,
    GAME_TELEPORT_CLOSE_X = 396, GAME_TELEPORT_CLOSE_Y = 102,
    GAME_TELEPORT_CLOSE_W = 32, GAME_TELEPORT_CLOSE_H = 32,
    /* End-of-run panels: shared by GAME_OVER and GAME_VICTORY. */
    GAME_END_PANEL_X = 80, GAME_END_PANEL_Y = 104,
    GAME_END_PANEL_W = 320, GAME_END_PANEL_H = 286,
    GAME_END_BUTTON_X = 140, GAME_END_BUTTON_W = 200,
    GAME_END_BUTTON_H = 42,
    GAME_END_BUTTON1_Y = 256, GAME_END_BUTTON2_Y = 310,
    GAME_END_BUTTON_Y = 284,
    /* The dungeon footer now carries two equal buttons: warp and restart. */
    GAME_FOOTER_BUTTONS = 2,
    GAME_FOOTER_X = (GAME_WIDTH - (2 * GAME_BUTTON_W + GAME_BUTTON_GAP)) / 2
};
