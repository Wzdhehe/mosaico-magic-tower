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
typedef enum {
    GAME_TITLE, GAME_DUNGEON, GAME_NEW_CONFIRM, GAME_SHOP, GAME_DIALOG,
    GAME_OVER, GAME_VICTORY, GAME_TELEPORT,
    /* Appended to preserve the existing screen identities. */
    GAME_RESTART_MENU
} game_screen_t;
typedef enum {
    GAME_NOTICE_NONE = 0, GAME_NOTICE_START, GAME_NOTICE_SAVED,
    GAME_NOTICE_NO_SAVE, GAME_NOTICE_DAMAGED, GAME_NOTICE_RESTORED,
    GAME_NOTICE_SAVE_FAILED, GAME_NOTICE_SUMMIT, GAME_NOTICE_CHAPTER
} game_notice_t;
typedef enum {
    GAME_GAIN_NONE = 0, GAME_GAIN_HP, GAME_GAIN_ATTACK, GAME_GAIN_DEFENCE,
    GAME_GAIN_KEY, GAME_GAIN_GOLD
} game_gain_t;
typedef enum { GAME_LANG_ZH = 0, GAME_LANG_EN = 1 } game_lang_t;
typedef enum {
    GAME_BATTERY_UNKNOWN, GAME_BATTERY_DISCHARGING,
    GAME_BATTERY_CHARGING
} game_battery_state_t;
typedef struct {
    tower_snapshot_t tower;
    game_screen_t screen;
    uint32_t tick;
    int32_t pointer_x, pointer_y;
    bool pointer_down, paused, has_save;
    /* True only when the active run has an auto-save checkpoint: the new run
     * wrote one, a real floor change wrote one, or the run was loaded from it. */
    bool has_checkpoint;
    bool settings_open, preferences_save_failed;
    uint8_t lang;
    uint8_t volume, brightness;
    uint8_t notice_id;
    uint8_t gain_kind;
    uint8_t gain_color;
    uint32_t gain_value;
    uint8_t music;
    uint8_t battery_percent;
    game_battery_state_t battery_state;
    /* Camera origin in floor tiles. Clamped so the viewport never leaves the
     * floor; both stay 0 on floors no larger than the viewport. */
    uint16_t camera_x, camera_y;
    uint16_t floor_w, floor_h;
    /* Selected anchor in the six-cell teleport panel (0..TOWER_ANCHORS-1). */
    uint8_t anchor_cursor;
    uint16_t dialogue_id;
    uint8_t dialogue_page, dialogue_variant;
} game_snapshot_t;
/* Snapshot is borrowed, read-only and owned by the single application task. */
bool game_create(const game_config_t *, game_handle_t *);
void game_delete(game_handle_t);
void game_reset(game_handle_t);
void game_set_paused(game_handle_t, bool);
/* Transient hardware telemetry, never part of the journey or preferences.
 * Call on the owning UI task. Only a visible change requests a frame. */
void game_set_battery(game_handle_t, unsigned percent, game_battery_state_t);
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
