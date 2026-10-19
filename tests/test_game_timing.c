// SPDX-License-Identifier: MIT
/* Timing, checkpoint and teleport regression coverage.
 *
 * The campaign maps are authored separately from this test, so the isolated
 * tests/tower_floors_stub.c fixture is linked instead. It keeps the same spawn
 * (1,1), dark floor (2,1), up stair (4,1) and chapter gate (8,1, or 17,1 on the
 * 19x19 final floor); floor 26 is 13x13. Its event tile sits at (5,1), away from
 * the production dialogue at (3,1) on floors 18 and 27. */
#include "game.h"
#include "game_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool save_ok = true, load_ok = true;
static int save_calls;
static uint8_t saved[TOWER_SAVE_BYTES];
static void music(int track) { (void)track; }
static void volume(int value) { (void)value; }
static bool brightness(int value) { (void)value; return true; }
static bool save_bytes(void *context, const uint8_t *bytes, size_t count)
{
    (void)context;
    if (!save_ok) return false;
    memcpy(saved, bytes, count);
    ++save_calls;
    return true;
}
static bool load_bytes(void *context, uint8_t *bytes, size_t count)
{
    (void)context;
    if (!load_ok) return false;
    memcpy(bytes, saved, count);
    return true;
}
static game_handle_t new_game(void)
{
    const game_config_t cfg = {.width=GAME_WIDTH, .height=GAME_HEIGHT,
        .save=save_bytes, .load=load_bytes, .set_music=music, .set_volume=volume,
        .set_brightness=brightness};
    game_handle_t game = NULL;
    assert(game_create(&cfg, &game) && game);
    return game;
}
static void press(game_handle_t game, int code)
{
    game_set_action(game, code, true);
    game_set_action(game, code, false);
}
static void tap(game_handle_t game, int x, int y)
{
    game_set_pointer(game, x, y, true);
    game_set_pointer(game, 0, 0, false);
}
static void settle(game_handle_t game)
{
    game_update(game);
    game_render_complete(game);
    assert(game_is_idle(game));
}
static game_handle_t new_started_game(void)
{
    game_handle_t game = new_game();
    save_ok = true;
    press(game, GAME_ACTION_START);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->has_checkpoint);
    settle(game);
    return game;
}
/* One floor of the stub: enter the dark floor, step onto the plain tile, take
 * the up stair. The fixture keeps its event tile off this path because the
 * campaign dispatches dialogue by floor+cell and a dialog would consume the
 * next key press. */
static void step_up(game_handle_t game)
{
    const unsigned floor = game_read(game)->tower.floor;
    for (int i = 0; i < 3; ++i) press(game, GAME_ACTION_RIGHT);
    if (game_read(game)->tower.floor != floor + 1u ||
        game_read(game)->tower.x != 1u || game_read(game)->tower.y != 1u) {
        const game_snapshot_t *s = game_read(game);
        fprintf(stderr, "step_up failed on floor %u: floor %u pos (%u,%u) "
                        "status %d screen %d\n",
                floor, s->tower.floor, s->tower.x, s->tower.y,
                (int)s->tower.status, (int)s->screen);
    }
    assert(game_read(game)->tower.floor == floor + 1u);
    assert(game_read(game)->tower.x == 1u && game_read(game)->tower.y == 1u);
}
static void climb_to_gate(game_handle_t game)
{
    while (game_read(game)->tower.floor < 9u) step_up(game);
}
/* From the gate floor's spawn, enter the dark floor, step past it, go down and
 * walk along row 2 to the goal on the top row. */
static void take_goal_x(game_handle_t game, unsigned x)
{
    press(game, GAME_ACTION_RIGHT); /* enter dark floor */
    press(game, GAME_ACTION_RIGHT); /* step onto the plain tile */
    press(game, GAME_ACTION_DOWN);
    while (game_read(game)->tower.x < x) press(game, GAME_ACTION_RIGHT);
    press(game, GAME_ACTION_UP);
}
static void take_gate(game_handle_t game) { take_goal_x(game, 8u); }
static bool saved_floor_is(unsigned floor)
{
    tower_handle_t check = NULL;
    tower_snapshot_t s;
    bool ok;
    if (tower_create(NULL, &check) != TOWER_OK) return false;
    ok = tower_import(check, saved, sizeof saved) == TOWER_OK &&
         tower_read(check, &s) == TOWER_OK && s.floor == floor;
    tower_delete(check);
    return ok;
}

static void test_new_game_checkpoints(void)
{
    game_handle_t game = new_started_game();
    assert(game_read(game)->notice_id == GAME_NOTICE_START);
    assert(game_read(game)->has_save && game_read(game)->has_checkpoint);
    assert(saved[4] == 8u); /* authored campaign save revision */
    assert(game_read(game)->tower.chapters == 0u);
    game_delete(game);
    puts("ok: a new run writes an initial checkpoint and has no manual save step");
}

static void test_no_manual_save_or_load_buttons(void)
{
    game_handle_t game = new_started_game();
    const int before = save_calls;
    /* The former footer save/load slots are warp and restart now. */
    tap(game, 187, 462);
    assert(game_read(game)->screen == GAME_TELEPORT);
    assert(game_read(game)->notice_id != GAME_NOTICE_SAVED);
    assert(save_calls == before);
    tap(game, 396 + 16, 102 + 16); /* close */
    assert(game_read(game)->screen == GAME_DUNGEON);
    tap(game, 292, 462);
    assert(game_read(game)->screen == GAME_RESTART_MENU);
    tap(game, GAME_RESTART_BUTTON_X+8, GAME_RESTART_CANCEL_Y+8);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(save_calls == before);
    game_delete(game);
    puts("ok: the dungeon footer has warp and restart, no save or load button");
}

static void test_floor_change_autosaves(void)
{
    game_handle_t game = new_started_game();
    const uint32_t start_floor = game_read(game)->tower.floor;
    step_up(game);
    assert(game_read(game)->notice_id == GAME_NOTICE_SAVED);
    assert(game_read(game)->tower.floor == start_floor + 1u);
    assert(saved_floor_is(start_floor + 1u));
    /* A same-floor action must not rewrite the checkpoint. */
    {
        uint8_t before[TOWER_SAVE_BYTES];
        memcpy(before, saved, sizeof before);
        press(game, GAME_ACTION_DOWN); /* floor tile, no floor change */
        assert(game_read(game)->tower.floor == start_floor + 1u);
        assert(!memcmp(before, saved, sizeof before));
    }
    game_delete(game);
    puts("ok: a real floor change autosaves; same-floor actions leave the save alone");
}

static void test_restart_menu_retry_restores_checkpoint(void)
{
    game_handle_t game = new_started_game();
    step_up(game); /* checkpoint on floor 1 */
    /* Spend a fight on the same floor: this must not move the checkpoint. */
    press(game, GAME_ACTION_DOWN);
    assert(game_read(game)->tower.phase == TOWER_PHASE_COMBAT);
    press(game, GAME_ACTION_START);
    assert(game_read(game)->tower.hp == 96u);
    const int writes = save_calls;

    tap(game, 292, 462);
    assert(game_read(game)->screen == GAME_RESTART_MENU);
    /* New game asks for confirmation and cancelling returns to this menu. */
    tap(game, GAME_RESTART_BUTTON_X + 8, GAME_RESTART_NEW_Y + 8);
    assert(game_read(game)->screen == GAME_NEW_CONFIRM);
    tap(game, 135, 265);
    assert(game_read(game)->screen == GAME_RESTART_MENU);
    tap(game, GAME_RESTART_BUTTON_X + 8, GAME_RESTART_CANCEL_Y + 8);
    assert(game_read(game)->screen == GAME_DUNGEON);

    tap(game, 292, 462);
    tap(game, GAME_RESTART_BUTTON_X + 8, GAME_RESTART_RETRY_Y + 8);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->notice_id == GAME_NOTICE_RESTORED);
    assert(game_read(game)->tower.floor == 1u);
    assert(game_read(game)->tower.x == 1u && game_read(game)->tower.y == 1u);
    assert(game_read(game)->tower.hp == 100u);
    assert(save_calls == writes); /* restoring a checkpoint does not rewrite it */
    settle(game);
    game_delete(game);
    puts("ok: restart menu retry restores the checkpoint; new game needs confirmation");
}

static void test_anchor_locks_until_chapter_cleared(void)
{
    game_handle_t game = new_started_game();
    tap(game, 187, 462);
    assert(game_read(game)->screen == GAME_TELEPORT);
    /* Six cells, 2x3: the second cell (anchor 10) is still locked. */
    tap(game, 252 + 20, 152 + 20);
    assert(game_read(game)->screen == GAME_TELEPORT);
    assert(game_read(game)->tower.floor == 0u);
    tap(game, 78 + 20, 152 + 20); /* anchor 1: current floor, closes */
    assert(game_read(game)->screen == GAME_DUNGEON);

    climb_to_gate(game);
    {
        uint8_t checkpoint[TOWER_SAVE_BYTES];
        memcpy(checkpoint, saved, sizeof checkpoint);
        take_gate(game);
        assert(game_read(game)->tower.chapters == 0x01u);
        assert(!game_read(game)->tower.cleared);
        assert(game_read(game)->tower.status == TOWER_STATUS_CHAPTER);
        /* Clearing the gate is not a floor change, so it does not write yet. */
        assert(!memcmp(checkpoint, saved, sizeof checkpoint));
    }

    tap(game, 187, 462);
    tap(game, 252 + 20, 152 + 20); /* anchor 10 is unlocked now */
    /* Picking the anchor you already stand on just closes the panel. */
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->tower.floor == 9u);
    tap(game, 187, 462);
    tap(game, 78 + 20, 152 + 20); /* down to floor 1 */
    assert(game_read(game)->tower.floor == 0u);
    tap(game, 187, 462);
    tap(game, 252 + 20, 152 + 20); /* and back up to the anchor */
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->tower.floor == 9u);
    assert(game_read(game)->tower.x == 1u && game_read(game)->tower.y == 1u);
    /* Those teleports changed floors, so the chapter mask was checkpointed. */
    {
        tower_handle_t check = NULL;
        tower_snapshot_t s;
        assert(tower_create(NULL, &check) == TOWER_OK);
        assert(tower_import(check, saved, sizeof saved) == TOWER_OK);
        assert(tower_read(check, &s) == TOWER_OK);
        assert(s.floor == 9u && s.chapters == 0x01u);
        tower_delete(check);
    }
    game_delete(game);
    puts("ok: chapter gate unlocks its anchor; teleport across floors checkpoints");
}

static void test_chapter_exit_stair_completes(void)
{
    game_handle_t game = new_started_game();
    climb_to_gate(game);
    assert(game_read(game)->tower.chapters == 0u);
    step_up(game); /* take the chapter's exit stair instead of the gate */
    assert(game_read(game)->tower.floor == 10u);
    assert(game_read(game)->tower.chapters == 0x01u);
    assert(game_read(game)->notice_id == GAME_NOTICE_CHAPTER);
    /* The stair transition itself autosaved the chapter state. */
    {
        tower_handle_t check = NULL;
        tower_snapshot_t s;
        assert(tower_create(NULL, &check) == TOWER_OK);
        assert(tower_import(check, saved, sizeof saved) == TOWER_OK);
        assert(tower_read(check, &s) == TOWER_OK);
        assert(s.floor == 10u && s.chapters == 0x01u);
        tower_delete(check);
    }
    /* Anchor 10 is now usable from here as well. */
    tap(game, 187, 462);
    tap(game, 78 + 20, 152 + 20);
    assert(game_read(game)->tower.floor == 0u);
    game_delete(game);
    puts("ok: leaving a chapter through its exit stair also unlocks the anchor");
}

static void test_death_retries_last_checkpoint(void)
{
    game_handle_t game = new_started_game();
    climb_to_gate(game);
    take_gate(game);
    tap(game, 187, 462);
    tap(game, 78 + 20, 152 + 20); /* back to floor 1 */
    assert(game_read(game)->tower.floor == 0u);

    /* Slime, then the knight that is lethal at the starting stats. */
    press(game, GAME_ACTION_DOWN);
    assert(game_read(game)->tower.phase == TOWER_PHASE_COMBAT);
    press(game, GAME_ACTION_START);
    assert(game_read(game)->tower.hp == 96u);
    {
        uint8_t before_death[TOWER_SAVE_BYTES];
        memcpy(before_death, saved, sizeof before_death);
        press(game, GAME_ACTION_DOWN);
        assert(game_read(game)->tower.phase == TOWER_PHASE_COMBAT);
        press(game, GAME_ACTION_START);
        assert(game_read(game)->screen == GAME_OVER);
        assert(!memcmp(before_death, saved, sizeof before_death));
    }
    assert(game_read(game)->has_checkpoint);
    tap(game, GAME_END_BUTTON_X + 20, GAME_END_BUTTON1_Y + 20);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->tower.hp == 100u);
    assert(game_read(game)->tower.floor == 0u);
    assert(game_read(game)->notice_id == GAME_NOTICE_RESTORED);
    game_delete(game);
    puts("ok: death never overwrites the checkpoint; retry restores it");
}

static void test_failed_write_is_reported(void)
{
    game_handle_t game = new_started_game();
    save_ok = false;
    tap(game, 292, 462);
    assert(game_read(game)->screen == GAME_RESTART_MENU);
    tap(game, GAME_RESTART_BUTTON_X+8, GAME_RESTART_NEW_Y+8);
    assert(game_read(game)->screen == GAME_NEW_CONFIRM);
    tap(game, 245, 265);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->notice_id == GAME_NOTICE_SAVE_FAILED);
    assert(!game_read(game)->has_checkpoint);
    save_ok = true;
    game_delete(game);
    puts("ok: a failed checkpoint write is reported and claims no checkpoint");
}

static void test_title_load_messages(void)
{
    game_handle_t game = new_started_game();
    game_delete(game); /* save_ok left a valid v8 record */

    load_ok = true;
    game = new_game();
    assert(game_read(game)->has_save);
    tap(game, 240, 340); /* continue journey */
    assert(game_read(game)->notice_id == GAME_NOTICE_RESTORED);
    assert(game_read(game)->screen == GAME_DUNGEON);
    game_delete(game);

    load_ok = false;
    game = new_game();
    tap(game, 240, 340);
    assert(game_read(game)->notice_id == GAME_NOTICE_NO_SAVE);
    assert(game_read(game)->screen == GAME_TITLE);
    game_delete(game);

    saved[30] ^= 0x40; /* checksum no longer matches */
    load_ok = true;
    game = new_game();
    tap(game, 240, 340);
    assert(game_read(game)->notice_id == GAME_NOTICE_DAMAGED);
    assert(game_read(game)->screen == GAME_TITLE);
    game_delete(game);
    saved[30] ^= 0x40;
    load_ok = true;
    puts("ok: continue reports restored, missing and damaged saves distinctly");
}

static void test_victory_returns_to_anchor(void)
{
    game_handle_t game = new_started_game();
    while (game_read(game)->tower.floor < 49u) step_up(game);
    assert(game_read(game)->tower.chapters == 0x0Fu);
    take_goal_x(game, 17u);
    assert(game_read(game)->screen == GAME_VICTORY);
    assert(game_read(game)->tower.cleared);
    assert(game_read(game)->tower.chapters == 0x1Fu);
    /* The summit screen keeps the run alive so the last anchor means something. */
    tap(game, GAME_END_BUTTON_X + 20, GAME_END_BUTTON_Y + 20);
    assert(game_read(game)->screen == GAME_DUNGEON);
    assert(game_read(game)->notice_id == GAME_NOTICE_SUMMIT);
    assert(game_read(game)->tower.floor == 49u);
    tap(game, 187, 462);
    tap(game, 252 + 20, 292 + 20); /* anchor 50: current floor, closes */
    assert(game_read(game)->screen == GAME_DUNGEON);
    tap(game, 187, 462);
    tap(game, 78 + 20, 152 + 20); /* back down to anchor 1 */
    assert(game_read(game)->tower.floor == 0u);
    tap(game, 187, 462);
    tap(game, 252 + 20, 292 + 20); /* and the last anchor is a real target */
    assert(game_read(game)->tower.floor == 49u);
    assert(game_read(game)->screen == GAME_DUNGEON);
    game_delete(game);
    puts("ok: clearing the tower returns to floor 50 and the last anchor works");
}

static void test_settings_and_idle_scheduling(void)
{
    game_handle_t game = new_started_game();
    assert(!game_needs_redraw(game));
    tap(game, GAME_SETTINGS_TOUCH_X + 8, GAME_SETTINGS_TOUCH_Y + 8);
    assert(game_read(game)->settings_open);
    assert(game_needs_redraw(game));
    game_update(game); game_render_complete(game);
    tap(game, GAME_SETTINGS_EN_X + 8, GAME_SETTINGS_EN_Y + 8);
    assert(game_read(game)->lang == GAME_LANG_EN);
    tap(game, GAME_SETTINGS_CLOSE_X + 8, GAME_SETTINGS_CLOSE_Y + 8);
    assert(!game_read(game)->settings_open);
    settle(game);
    /* An idle run neither requests a redraw nor advances the scheduler. */
    for (int i = 0; i < 300; ++i) {
        game_update(game);
        assert(game_is_idle(game));
        assert(!game_needs_redraw(game));
    }
    /* A directional press repeats only while held: the rising edge moves once,
     * then one repeat lands after GAME_KEY_REPEAT updates. */
    press(game, GAME_ACTION_RIGHT);
    assert(game_read(game)->tower.turns == 1u); /* dark floor entry */
    game_set_action(game, GAME_ACTION_RIGHT, true);
    assert(game_read(game)->tower.turns == 2u); /* rising edge */
    for (int i = 0; i < GAME_KEY_REPEAT; ++i) game_update(game);
    assert(game_read(game)->tower.turns == 3u);
    game_set_action(game, GAME_ACTION_RIGHT, false);
    settle(game);
    game_delete(game);
    puts("ok: settings overlay, redraw scheduling and held-repeat timing still hold");
}

int main(void)
{
    assert(GAME_TICK_HZ == 15 && GAME_KEY_REPEAT == 4);
    test_new_game_checkpoints();
    test_no_manual_save_or_load_buttons();
    test_floor_change_autosaves();
    test_restart_menu_retry_restores_checkpoint();
    test_anchor_locks_until_chapter_cleared();
    test_chapter_exit_stair_completes();
    test_death_retries_last_checkpoint();
    test_victory_returns_to_anchor();
    test_failed_write_is_reported();
    test_title_load_messages();
    test_settings_and_idle_scheduling();
    puts("PASS: autosave, six anchored warp points, chapter unlocks, retry and timing");
    return 0;
}
