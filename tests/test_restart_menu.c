// SPDX-License-Identifier: MIT
/* Real game/tower models with a deterministic, event-free map fixture. */
#include "game.h"
#include "game_config.h"
#include "tower_floors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *const rows[19] = {
    "###################", "#Dyap............U#", "#k................#",
    "#.................#", "#.................#", "#.................#",
    "#.................#", "#.................#", "#.................#",
    "#.................#", "#.................#", "#.................#",
    "#.................#", "#.................#", "#.................#",
    "#.................#", "#.................#", "#.................#",
    "###################"
};
#define FLOOR {19u, 19u, rows}
#define TEN FLOOR, FLOOR, FLOOR, FLOOR, FLOOR, FLOOR, FLOOR, FLOOR, FLOOR, FLOOR
const tower_floor_desc_t tower_floor_table[TOWER_FLOORS] = {TEN, TEN, TEN, TEN, TEN};
/* Append-only screen identity: the eight pre-existing values stay unchanged. */
_Static_assert(GAME_TELEPORT == 7 && GAME_RESTART_MENU == 8,
               "restart screen must preserve existing screen identities");
#define RESTART_SCREEN GAME_RESTART_MENU

static uint8_t saved[TOWER_SAVE_BYTES];
static bool save_ok, load_ok;
static unsigned saves, loads;
static bool save_data(void *context, const uint8_t *bytes, size_t size)
{
    (void)context;
    ++saves;
    if (!save_ok) return false;
    memcpy(saved, bytes, size);
    return true;
}
static bool load_data(void *context, uint8_t *bytes, size_t size)
{
    (void)context;
    ++loads;
    if (!load_ok) return false;
    memcpy(bytes, saved, size);
    return true;
}
static void key(game_handle_t g, int code)
{ game_set_action(g, code, true); game_set_action(g, code, false); }
static void tap(game_handle_t g, int x, int y)
{ game_set_pointer(g, x, y, true); game_set_pointer(g, 0, 0, false); }
static void restart(game_handle_t g)
{ tap(g, 292, 462); assert(game_read(g)->screen == RESTART_SCREEN); }
static game_handle_t started(bool storage)
{
    game_handle_t g = NULL;
    const game_config_t config = {.width=GAME_WIDTH, .height=GAME_HEIGHT,
        .save=save_data, .load=load_data};
    save_ok=storage; load_ok=false; saves=loads=0;
    assert(game_create(&config, &g));
    key(g, GAME_ACTION_START);
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert(game_read(g)->has_checkpoint == storage);
    load_ok=true;
    return g;
}
static void assert_run_unchanged(game_handle_t g, const game_snapshot_t *before)
{
    const game_snapshot_t *after=game_read(g);
    assert(memcmp(&before->tower, &after->tower, sizeof before->tower) == 0);
    assert(after->camera_x == before->camera_x && after->camera_y == before->camera_y);
    assert(after->has_checkpoint == before->has_checkpoint);
    assert(after->has_save == before->has_save);
}
static void explore(game_handle_t g)
{
    for (unsigned i=0; i<14; ++i) key(g, GAME_ACTION_RIGHT);
    assert(game_read(g)->tower.x == 15u);
    assert(game_read(g)->camera_x > 0u);
    assert(game_read(g)->tower.keys[0] == 1u);
}
static void test_cancel_and_new_confirm(void)
{
    game_handle_t g=started(true);
    explore(g);
    const game_snapshot_t before=*game_read(g);
    const unsigned writes=saves;
    restart(g);
    tap(g, 240, 329); /* cancel */
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert_run_unchanged(g, &before);
    restart(g);
    tap(g, 240, 273); /* new */
    assert(game_read(g)->screen == GAME_NEW_CONFIRM);
    assert_run_unchanged(g, &before);
    tap(g, 185, 283); /* back */
    assert(game_read(g)->screen == RESTART_SCREEN);
    key(g, GAME_ACTION_DOWN);
    assert(game_read(g)->screen == GAME_NEW_CONFIRM);
    key(g, GAME_ACTION_RESET);
    assert(game_read(g)->screen == RESTART_SCREEN);
    key(g, GAME_ACTION_RESET);
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert_run_unchanged(g, &before);
    assert(saves == writes);
    key(g, GAME_ACTION_RESET);
    assert(game_read(g)->screen == RESTART_SCREEN);
    key(g, GAME_ACTION_DOWN);
    key(g, GAME_ACTION_START);
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert(game_read(g)->tower.x == 1u && game_read(g)->tower.keys[0] == 0u);
    assert(game_read(g)->camera_x == 0u);
    assert(saves == writes+1u && game_read(g)->has_checkpoint);
    game_delete(g);
    puts("ok: cancel/reset preserve the run; only confirmed new game writes");
}
static void test_retry_success_and_failures(void)
{
    game_handle_t g=started(true);
    uint8_t checkpoint[TOWER_SAVE_BYTES];
    memcpy(checkpoint, saved, sizeof checkpoint);
    explore(g);
    const game_snapshot_t before=*game_read(g);
    const unsigned writes=saves;
    restart(g);
    load_ok=false;
    key(g, GAME_ACTION_START);
    assert(game_read(g)->screen == RESTART_SCREEN);
    assert(game_read(g)->notice_id == GAME_NOTICE_NO_SAVE);
    assert_run_unchanged(g, &before);
    assert(saves == writes);
    assert(memcmp(saved, checkpoint, sizeof saved) == 0);
    load_ok=true;
    saved[TOWER_SAVE_BYTES-1] ^= 1u;
    uint8_t damaged[TOWER_SAVE_BYTES];
    memcpy(damaged, saved, sizeof damaged);
    tap(g, 240, 217);
    assert(game_read(g)->screen == RESTART_SCREEN);
    assert(game_read(g)->notice_id == GAME_NOTICE_DAMAGED);
    assert_run_unchanged(g, &before);
    assert(saves == writes);
    assert(memcmp(saved, damaged, sizeof saved) == 0);
    memcpy(saved, checkpoint, sizeof saved);
    key(g, GAME_ACTION_START);
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert(game_read(g)->notice_id == GAME_NOTICE_RESTORED);
    assert(game_read(g)->tower.x == 1u && game_read(g)->tower.keys[0] == 0u);
    assert(game_read(g)->camera_x == 0u && saves == writes);
    game_delete(g);
    puts("ok: retry restores checkpoint; missing/corrupt loads preserve live state and writes");
}
static void test_disabled_retry(void)
{
    game_handle_t g=started(false);
    restart(g);
    const unsigned reads=loads, writes=saves;
    key(g, GAME_ACTION_START);
    tap(g, 240, 217);
    assert(game_read(g)->screen == RESTART_SCREEN);
    assert(loads == reads && saves == writes);
    key(g, GAME_ACTION_LEFT);
    assert(game_read(g)->screen == GAME_DUNGEON);
    restart(g);
    tap(g, 240, 273);
    assert(game_read(g)->screen == GAME_NEW_CONFIRM);
    save_ok=true;
    tap(g, 295, 283);
    assert(game_read(g)->screen == GAME_DUNGEON);
    assert(saves == writes+1u && game_read(g)->has_checkpoint);
    game_delete(g);
    puts("ok: retry is disabled without an active checkpoint");
}
static void die(game_handle_t g)
{
    key(g, GAME_ACTION_DOWN);
    assert(game_read(g)->tower.phase == TOWER_PHASE_COMBAT);
    key(g, GAME_ACTION_START);
    assert(game_read(g)->screen == GAME_OVER);
    game_update(g);
    assert(game_read(g)->music == 9u);
}
static void test_death_confirmation_and_menu_source(void)
{
    for (unsigned checkpoint=0; checkpoint<2; ++checkpoint) {
        game_handle_t g=started(checkpoint != 0u);
        die(g);
        const unsigned writes=saves;
        const game_snapshot_t dead=*game_read(g);
        tap(g, 240, checkpoint ? 331 : 305);
        assert(game_read(g)->screen == GAME_NEW_CONFIRM);
        game_update(g);
        assert(game_read(g)->music == 9u);
        assert_run_unchanged(g, &dead);
        key(g, GAME_ACTION_RESET);
        assert(game_read(g)->screen == GAME_OVER);
        key(g, GAME_ACTION_RESET);
        assert(game_read(g)->screen == RESTART_SCREEN);
        game_update(g);
        assert(game_read(g)->music == 9u);
        if (checkpoint) {
            load_ok=false;
            key(g, GAME_ACTION_START);
            assert(game_read(g)->screen == RESTART_SCREEN);
            assert(game_read(g)->notice_id == GAME_NOTICE_NO_SAVE);
            assert_run_unchanged(g, &dead);
        }
        key(g, GAME_ACTION_LEFT);
        assert(game_read(g)->screen == GAME_OVER);
        assert_run_unchanged(g, &dead);
        assert(saves == writes);
        if (!checkpoint) {
            key(g, GAME_ACTION_START);
            assert(game_read(g)->screen == GAME_NEW_CONFIRM);
            assert(saves == writes);
            key(g, GAME_ACTION_LEFT);
            assert(game_read(g)->screen == GAME_OVER);
        }
        game_delete(g);
    }
    puts("ok: death-page new game requires confirmation; cancel returns to its source");
}
static void test_battery_redraw(void)
{
    game_handle_t g=started(true);
    game_update(g);
    game_render_complete(g);
    const uint32_t hash=game_hash(g), turns=game_read(g)->tower.turns;
    const unsigned writes=saves;
    assert(game_read(g)->battery_state==GAME_BATTERY_UNKNOWN);
    game_set_battery(g,67,GAME_BATTERY_CHARGING);
    assert(game_needs_redraw(g));
    assert(game_read(g)->battery_percent==67u);
    game_render_complete(g);
    for(unsigned i=0;i<300;++i) {
        game_set_battery(g,67,GAME_BATTERY_CHARGING);
        game_update(g);
        assert(game_is_idle(g));
    }
    assert(game_hash(g)==hash && game_read(g)->tower.turns==turns && saves==writes);
    game_set_battery(g,67,GAME_BATTERY_DISCHARGING);
    assert(game_needs_redraw(g));
    game_render_complete(g);
    game_set_battery(g,100,GAME_BATTERY_CHARGING);
    assert(game_read(g)->battery_state==GAME_BATTERY_CHARGING && game_needs_redraw(g));
    game_set_battery(g,255,GAME_BATTERY_CHARGING);
    assert(game_read(g)->battery_state==GAME_BATTERY_UNKNOWN);
    assert(game_read(g)->battery_percent==0u);
    game_render_complete(g);
    game_set_battery(g,44,GAME_BATTERY_UNKNOWN);
    assert(!game_needs_redraw(g));
    game_reset(g);
    assert(game_read(g)->battery_state==GAME_BATTERY_UNKNOWN);
    game_delete(g);
    puts("ok: battery redraws on visible changes only; no turns, saves or idle ticks");
}
int main(void)
{
    test_battery_redraw();
    test_cancel_and_new_confirm();
    test_retry_success_and_failures();
    test_disabled_retry();
    test_death_confirmation_and_menu_source();
    puts("PASS: restart menu regression");
    return 0;
}
