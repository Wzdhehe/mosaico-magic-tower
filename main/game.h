// SPDX-License-Identifier: MIT
#pragma once
#include "tower.h"
typedef struct game_t *game_handle_t;
typedef bool (*game_save_fn)(void *, const uint8_t *, size_t);
typedef bool (*game_load_fn)(void *, uint8_t *, size_t);
typedef struct {
    uint8_t lang, volume, brightness;
} game_preferences_t;

typedef struct {
    uint16_t width, height;
    game_save_fn save;
    game_load_fn load;
    void *storage_context;
    void (*set_volume)(int);
    void (*set_music)(int);
    bool (*load_preferences)(void *, game_preferences_t *);
    bool (*save_preferences)(void *, const game_preferences_t *);
    void *preferences_context;
    bool (*set_brightness)(int);
} game_config_t;
typedef enum { GAME_TITLE, GAME_DUNGEON, GAME_NEW_CONFIRM, GAME_SHOP, GAME_DIALOG } game_screen_t;
typedef enum {
    GAME_NOTICE_NONE = 0, GAME_NOTICE_START, GAME_NOTICE_SAVED,
    GAME_NOTICE_NO_SAVE, GAME_NOTICE_DAMAGED, GAME_NOTICE_RESTORED,
    GAME_NOTICE_SAVE_FAILED, GAME_NOTICE_SUMMIT
} game_notice_t;
typedef enum {
    GAME_GAIN_NONE = 0, GAME_GAIN_HP, GAME_GAIN_ATTACK, GAME_GAIN_DEFENCE,
    GAME_GAIN_KEY, GAME_GAIN_GOLD
} game_gain_t;
typedef enum { GAME_LANG_ZH = 0, GAME_LANG_EN = 1 } game_lang_t;
typedef struct {
    tower_snapshot_t tower;
    game_screen_t screen;
    uint32_t tick;
    int32_t pointer_x, pointer_y;
    bool pointer_down, paused, has_save;
    bool settings_open, preferences_save_failed;
    uint8_t lang;
    uint8_t volume, brightness;
    uint8_t notice_id;
    uint8_t gain_kind;
    uint8_t gain_color;
    uint32_t gain_value;
    uint8_t music;
} game_snapshot_t;
/* Snapshot is borrowed, read-only and owned by the single application task. */
bool game_create(const game_config_t *, game_handle_t *);
void game_delete(game_handle_t);
void game_reset(game_handle_t);
void game_set_paused(game_handle_t, bool);
void game_set_pointer(game_handle_t, int32_t, int32_t, bool);
void game_set_action(game_handle_t, int, bool);
void game_update(game_handle_t);
/* Instance-owned display scheduling; complete only a successful presentation.
 * Request redraw again when a platform reports submission failure afterward. */
bool game_needs_redraw(game_handle_t);
void game_render_complete(game_handle_t);
void game_request_redraw(game_handle_t);
bool game_is_idle(game_handle_t);
/* Forced rendering fills a fresh screenshot target even when settled. */
bool game_render(game_handle_t);
/* Device rendering avoids frame acquisition while no operation is pending. */
bool game_render_pending(game_handle_t);
const game_snapshot_t *game_read(game_handle_t);
tower_tile_t game_tile(game_handle_t, int, int);
uint32_t game_hash(game_handle_t);
