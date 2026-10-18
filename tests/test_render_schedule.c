// SPDX-License-Identifier: MIT
// Link the real RGB565 renderer; only frame acquisition/presentation is a spy.
#include "game.h"
#include "game_config.h"
#include "game_device_render.h"
#include "mosaico_game.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t pixels[GAME_WIDTH * GAME_HEIGHT];
static unsigned acquisitions, presents;
static bool fail_acquire;
static bool fail_present;
static mosaico_game_stats_t stats;

void MosaicoGameGetStats(mosaico_game_stats_t *out) { *out = stats; }

esp_err_t mosaico_raylib_port_begin_frame(uint16_t **out_pixels, size_t *stride)
{
    ++acquisitions;
    if (fail_acquire) return ESP_ERR_INVALID_STATE;
    *out_pixels = pixels;
    *stride = GAME_WIDTH;
    return ESP_OK;
}
esp_err_t mosaico_raylib_port_present_frame(void)
{
    ++presents;
    if (fail_present) {
        ++stats.dropped_frames;
        ++stats.superseded_frames;
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}
static uint32_t image_hash(void)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(pixels)/sizeof(pixels[0]); ++i)
        hash = (hash ^ pixels[i]) * 16777619u;
    return hash;
}
static void press(game_handle_t game, int code)
{
    game_set_action(game, code, true);
    game_set_action(game, code, false);
    game_update(game);
}
static void tap(game_handle_t game, int x, int y)
{
    game_set_pointer(game, x, y, true);
    game_set_pointer(game, x, y, false);
    game_update(game);
}
static void paint_once(game_handle_t game)
{
    unsigned previous = presents;
    assert(game_needs_redraw(game));
    assert(game_render_device_pending(game));
    assert(presents == previous + 1);
    assert(!game_needs_redraw(game));
}
int main(void)
{
    const game_config_t config = {.width=GAME_WIDTH, .height=GAME_HEIGHT};
    game_handle_t game = NULL;
    assert(game_create(&config, &game));
    InitWindow(GAME_WIDTH, GAME_HEIGHT, "render test");
    assert(!game_is_idle(game));
    fail_acquire = true;
    assert(!game_render_device_pending(game));
    assert(game_needs_redraw(game));
    assert(presents == 0);
    fail_acquire = false;
    fail_present = true;
    assert(!game_render_device_pending(game));
    assert(game_needs_redraw(game) && !game_is_idle(game));
    assert(presents == 1);
    fail_present = false;
    paint_once(game);
    /* The unified settings entry frees the old top-left speaker button. */
    assert(pixels[15 * GAME_WIDTH + 21] == (uint16_t)((14u >> 3) << 11 | (19u >> 2) << 5 | (28u >> 3)));
    assert(game_is_idle(game));
    unsigned idle_acquisitions = acquisitions, idle_presents = presents;
    uint32_t title_hash = image_hash();
    for (int i=0; i<300; ++i) {
        game_update(game);
        assert(game_is_idle(game));
        assert(game_render_pending(game));
    }
    assert(acquisitions == idle_acquisitions && presents == idle_presents);
    assert(image_hash() == title_hash);

    // Explicit screenshots fill a fresh target even after the frame is clean.
    memset(pixels, 0, sizeof(pixels));
    assert(game_render(game));
    assert(image_hash() == title_hash);
    press(game, GAME_ACTION_START);
    paint_once(game);
    press(game, GAME_ACTION_UP);
    paint_once(game);
    assert(game_read(game)->gain_kind == GAME_GAIN_HP);
    const uint32_t dungeon_hash = image_hash();
    idle_acquisitions = acquisitions;
    idle_presents = presents;
    for (int i=0; i<300; ++i) {
        game_update(game);
        assert(game_render_pending(game));
    }
    assert(game_read(game)->gain_kind == GAME_GAIN_HP);
    assert(image_hash() == dungeon_hash);
    assert(acquisitions == idle_acquisitions && presents == idle_presents);

    // Background pointer sampling has no visible feedback to redraw.
    game_set_pointer(game, 240, 348, true);
    for (int i=0; i<30; ++i) {
        game_set_pointer(game, 240, 348, true);
        assert(game_render_pending(game));
    }
    assert(presents == idle_presents);
    game_set_pointer(game, 240, 348, false);

    // An unchanged failed move must not animate the NPCs/enemies.
    press(game, GAME_ACTION_DOWN);
    press(game, GAME_ACTION_DOWN);
    assert(game_render(game));
    const uint32_t blocked_hash = image_hash();
    uint32_t before_tick = game_read(game)->tick;
    game_set_action(game, GAME_ACTION_DOWN, true);
    for (int i=0; i<40; ++i) {
        game_update(game);
        assert(game_render(game));
        assert(image_hash() == blocked_hash);
    }
    game_set_action(game, GAME_ACTION_DOWN, false);
    assert(game_read(game)->tick > before_tick);

    tap(game, GAME_SETTINGS_X+20, GAME_SETTINGS_Y+15);
    paint_once(game);
    assert(game_read(game)->settings_open);
    tap(game, GAME_SETTINGS_EN_X+20, GAME_SETTINGS_EN_Y+15);
    paint_once(game);
    assert(game_read(game)->lang==GAME_LANG_EN);
    game_set_pointer(game, GAME_SETTINGS_VOLUME_X+GAME_SETTINGS_VOLUME_W/2, GAME_SETTINGS_VOLUME_Y+5, true);
    paint_once(game);
    idle_presents = presents;
    for (int i=0; i<30; ++i) {
        game_set_pointer(game, GAME_SETTINGS_VOLUME_X+GAME_SETTINGS_VOLUME_W/2, GAME_SETTINGS_VOLUME_Y+5, true);
        assert(game_render_pending(game));
    }
    assert(presents == idle_presents);
    game_set_pointer(game, 0, 0, false);
    game_set_pointer(game,GAME_SETTINGS_BRIGHTNESS_X,GAME_SETTINGS_BRIGHTNESS_Y+5,true);
    game_update(game);
    paint_once(game);
    assert(game_read(game)->brightness==GAME_BRIGHTNESS_MIN);
    game_set_pointer(game,0,0,false);
    idle_acquisitions=acquisitions; idle_presents=presents;
    game_set_action(game,GAME_ACTION_RIGHT,true);
    for(int i=0;i<300;++i) {
        game_update(game);
        assert(game_is_idle(game));
        assert(game_render_device_pending(game));
    }
    game_set_action(game,GAME_ACTION_RIGHT,false);
    assert(acquisitions==idle_acquisitions && presents==idle_presents);
    tap(game,GAME_SETTINGS_CLOSE_X+10,GAME_SETTINGS_CLOSE_Y+10);
    paint_once(game);
    assert(!game_read(game)->settings_open);
    game_delete(game);
    CloseWindow();
    puts("PASS: real RGB565 acquire/submit retry; 300 idle polls = 0 acquisitions/presents; input, static sprites across active ticks, sticky feedback and fresh-target screenshots");
    return 0;
}
