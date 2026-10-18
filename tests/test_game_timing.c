// SPDX-License-Identifier: MIT
// Portable scheduling and input regression coverage; no Mosaico BSP required.
#include "game.h"
#include "game_config.h"
#include "win_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int music_calls, last_music, volume_calls;
static bool save_ok, load_ok;
static uint8_t saved[TOWER_SAVE_BYTES];
static void music(int track) { ++music_calls; last_music=track; }
static void volume(int value) { (void)value; ++volume_calls; }
static bool save_bytes(void *context, const uint8_t *bytes, size_t count)
{ (void)context; if (!save_ok) return false; memcpy(saved,bytes,count); return true; }
static bool load_bytes(void *context, uint8_t *bytes, size_t count)
{ (void)context; if (!load_ok) return false; memcpy(bytes,saved,count); return true; }
static game_handle_t new_game(void)
{
    const game_config_t cfg = {.width=GAME_WIDTH, .height=GAME_HEIGHT,
        .save=save_bytes,.load=load_bytes,.set_music=music,.set_volume=volume};
    game_handle_t game=NULL;
    assert(game_create(&cfg,&game) && game);
    return game;
}
static void press(game_handle_t game, int code)
{ game_set_action(game,code,true); game_set_action(game,code,false); }
static void tap(game_handle_t game, int x, int y)
{ game_set_pointer(game,x,y,true); game_set_pointer(game,0,0,false); }
static void settle(game_handle_t game)
{ game_update(game); game_render_complete(game); assert(game_is_idle(game)); }
static game_handle_t new_started_game(void)
{
    game_handle_t game=new_game();
    press(game,GAME_ACTION_START);
    assert(game_read(game)->screen==GAME_DUNGEON);
    settle(game);
    return game;
}
static void test_pending_and_idle(void)
{
    assert(!game_needs_redraw(NULL));
    assert(game_is_idle(NULL));
    game_render_complete(NULL);
    game_request_redraw(NULL);
    game_handle_t game=new_game(), other=new_game();
    assert(game_needs_redraw(game) && !game_is_idle(game));
    game_render_complete(game);
    assert(!game_needs_redraw(game) && game_is_idle(game));
    game_request_redraw(game);
    assert(game_needs_redraw(game) && !game_is_idle(game));
    game_render_complete(game);
    assert(game_needs_redraw(other));
    const uint32_t initial=game_hash(game), tick=game_read(game)->tick;
    for (int i=0;i<1000;++i) game_update(game);
    assert(game_hash(game)==initial && game_read(game)->tick==tick);
    assert(!game_needs_redraw(game) && game_is_idle(game));
    game_set_pointer(game,200,200,true);
    game_set_pointer(game,220,200,true);
    assert(!game_needs_redraw(game) && game_is_idle(game));
    game_set_pointer(game,0,0,false);
    assert(!game_needs_redraw(game));
    press(game,GAME_ACTION_START);
    assert(game_needs_redraw(game));
    game_render_complete(game);
    const int calls=music_calls;
    assert(!game_is_idle(game));
    game_update(game);
    assert(music_calls==calls+1 && last_music==1);
    assert(game_is_idle(game) && !game_needs_redraw(game));
    game_delete(other); game_delete(game);
}
static void test_sticky_feedback_and_storage(void)
{
    game_handle_t game=new_started_game();
    assert(game_read(game)->notice_id==GAME_NOTICE_START);
    for (int i=0;i<1000;++i) game_update(game);
    assert(game_read(game)->notice_id==GAME_NOTICE_START && game_is_idle(game));
    press(game,GAME_ACTION_UP);
    assert(game_read(game)->tower.y==8 && game_needs_redraw(game));
    assert(game_read(game)->notice_id==GAME_NOTICE_NONE);
    assert(game_read(game)->gain_kind==GAME_GAIN_HP);
    settle(game);
    for (int i=0;i<1000;++i) game_update(game);
    assert(game_read(game)->gain_kind==GAME_GAIN_HP && game_is_idle(game));
    save_ok=false; tap(game,135,462);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_SAVE_FAILED);
    assert(game_read(game)->gain_kind==GAME_GAIN_NONE);
    settle(game);
    for (int i=0;i<1000;++i) game_update(game);
    assert(game_read(game)->notice_id==GAME_NOTICE_SAVE_FAILED);
    save_ok=true; tap(game,135,462);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_SAVED);
    settle(game);
    load_ok=false; tap(game,240,462);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_NO_SAVE);
    settle(game);
    load_ok=true; saved[30]^=0x40; tap(game,240,462);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_DAMAGED);
    saved[30]^=0x40; settle(game);
    tap(game,240,462);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_RESTORED);
    settle(game);
    press(game,GAME_ACTION_LEFT);
    assert(game_needs_redraw(game) && game_read(game)->notice_id==GAME_NOTICE_NONE);
    assert(game_read(game)->gain_kind==GAME_GAIN_NONE);
    game_delete(game); load_ok=false;
}
static void test_pointer_volume_pause_reset_and_dialog(void)
{
    game_handle_t game=new_started_game();
    game_set_pointer(game,240,350,true);
    for (int i=0;i<10;++i) game_set_pointer(game,240,350,true);
    assert(game_is_idle(game) && !game_needs_redraw(game));
    game_set_pointer(game,0,0,false);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==8);
    settle(game);
    tap(game,437,18);
    assert(game_needs_redraw(game) && game_read(game)->lang==GAME_LANG_EN);
    settle(game);
    tap(game,50,30);
    assert(game_needs_redraw(game) && game_read(game)->volume_open);
    settle(game);
    const int calls=volume_calls;
    game_set_pointer(game,240,230,true);
    assert(game_needs_redraw(game) && game_read(game)->volume==50);
    game_render_complete(game);
    for (int i=0;i<10;++i) game_set_pointer(game,240,230,true);
    assert(game_is_idle(game) && !game_needs_redraw(game) && volume_calls==calls+1);
    game_set_pointer(game,242,230,true);
    assert(!game_needs_redraw(game) && volume_calls==calls+1);
    game_set_pointer(game,244,230,true);
    assert(game_needs_redraw(game) && game_read(game)->volume==51);
    game_set_pointer(game,0,0,false); settle(game);
    tap(game,240,285);
    assert(game_needs_redraw(game) && !game_read(game)->volume_open);
    settle(game);
    game_set_paused(game,true);
    assert(game_needs_redraw(game)); settle(game);
    game_set_paused(game,true); press(game,GAME_ACTION_UP);
    assert(game_is_idle(game) && !game_needs_redraw(game));
    game_set_paused(game,false);
    assert(game_needs_redraw(game)); settle(game);
    game_reset(game);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_NEW_CONFIRM);
    settle(game); tap(game,180,280);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_DUNGEON);
    settle(game); game_reset(game); tap(game,285,280);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==9);
    settle(game); press(game,GAME_ACTION_RIGHT);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_DIALOG);
    settle(game); press(game,GAME_ACTION_START);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_DUNGEON);
    game_delete(game);
}
static void test_held_repeat_and_combat(void)
{
    game_handle_t game=new_started_game();
    game_set_action(game,GAME_ACTION_PAUSE,true);
    assert(game_read(game)->paused && game_needs_redraw(game));
    game_render_complete(game);
    game_set_action(game,GAME_ACTION_PAUSE,true);
    assert(game_read(game)->paused && !game_needs_redraw(game));
    game_set_action(game,GAME_ACTION_PAUSE,false);
    press(game,GAME_ACTION_PAUSE);
    assert(!game_read(game)->paused);
    game_set_action(game,GAME_ACTION_RESET,true);
    assert(game_read(game)->screen==GAME_NEW_CONFIRM);
    game_render_complete(game);
    game_set_action(game,GAME_ACTION_RESET,true);
    assert(!game_needs_redraw(game));
    game_set_action(game,GAME_ACTION_RESET,false);
    press(game,GAME_ACTION_LEFT); settle(game);
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_read(game)->tower.y==8);
    game_render_complete(game);
    assert(!game_is_idle(game));
    game_set_action(game,GAME_ACTION_UP,true);
    assert(!game_needs_redraw(game));
    for(int i=0;i<GAME_KEY_REPEAT-1;++i) game_update(game);
    assert(game_read(game)->tower.y==8 && !game_needs_redraw(game));
    game_update(game);
    assert(game_read(game)->tower.y==7 && game_needs_redraw(game));
    game_set_action(game,GAME_ACTION_UP,false); settle(game);
    for(int i=0;i<GAME_KEY_REPEAT*2;++i) game_update(game);
    assert(game_read(game)->tower.y==7 && game_is_idle(game));
    game_delete(game);
    game=new_started_game();
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_read(game)->tower.y==8);
    game_set_paused(game,true); game_render_complete(game);
    game_set_paused(game,false); game_render_complete(game);
    for (int i=0;i<100;++i) { game_set_action(game,GAME_ACTION_UP,true); game_update(game); }
    assert(game_read(game)->tower.y==8 && game_is_idle(game) && !game_needs_redraw(game));
    game_set_action(game,GAME_ACTION_UP,false); press(game,GAME_ACTION_UP);
    assert(game_read(game)->tower.y==7 && game_needs_redraw(game));
    game_delete(game);
    game=new_started_game();
    press(game,GAME_ACTION_UP); press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT);
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_needs_redraw(game) && game_read(game)->tower.phase==TOWER_PHASE_COMBAT);
    settle(game);
    press(game,GAME_ACTION_START);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==7);
    settle(game);
    const uint32_t turns=game_read(game)->tower.turns;
    for (int i=0;i<100;++i) { game_set_action(game,GAME_ACTION_UP,true); game_update(game); }
    assert(game_is_idle(game) && !game_needs_redraw(game) && game_read(game)->tower.turns==turns);
    game_set_action(game,GAME_ACTION_UP,false); press(game,GAME_ACTION_UP);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==6);
    game_delete(game);
    game=new_started_game();
    press(game,GAME_ACTION_UP); press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT);
    press(game,GAME_ACTION_UP); settle(game);
    tap(game,180,298);
    assert(game_needs_redraw(game) && game_read(game)->tower.phase==TOWER_PHASE_PLAY);
    assert(game_read(game)->tower.y==8);
    game_delete(game);
}
static void test_route_music(void)
{
    game_handle_t game=new_started_game();
    bool saw_upper_music=false;
    for (unsigned i=0;i<WIN_ROUTE_COUNT;++i) {
        press(game,WIN_ROUTE[i]);
        game_render_complete(game);
        const uint8_t expected=game_read(game)->tower.floor>=5?2:1;
        if (game_read(game)->music!=expected) assert(!game_is_idle(game));
        game_update(game);
        assert(game_read(game)->music==expected && last_music==expected);
        if (expected==2) saw_upper_music=true;
        game_render_complete(game);
        assert(game_is_idle(game));
    }
    assert(saw_upper_music && game_read(game)->tower.cleared);
    game_delete(game);
}
static void walking_press(game_handle_t game, int code)
{
    press(game,code);
    if (game_read(game)->tower.phase==TOWER_PHASE_COMBAT || game_read(game)->screen==GAME_DIALOG)
        press(game,GAME_ACTION_START);
}
static void test_shop_transitions_and_feedback(void)
{
    game_handle_t game=new_started_game();
    for (unsigned i=0;i<WIN_ROUTE_COUNT && game_read(game)->tower.floor==0;++i)
        press(game,WIN_ROUTE[i]);
    assert(game_read(game)->tower.floor==1 && game_read(game)->tower.y==9);
    /* Collect this floor's yellow key, then take the optional merchant detour. */
    walking_press(game,GAME_ACTION_UP);
    for (int i=0;i<4;++i) walking_press(game,GAME_ACTION_LEFT);
    walking_press(game,GAME_ACTION_UP);
    for (int i=0;i<4;++i) walking_press(game,GAME_ACTION_RIGHT);
    walking_press(game,GAME_ACTION_UP); walking_press(game,GAME_ACTION_UP);
    for (int i=0;i<4;++i) walking_press(game,GAME_ACTION_LEFT);
    settle(game); save_ok=true; tap(game,135,462);
    walking_press(game,GAME_ACTION_UP);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_SHOP);
    settle(game);
    uint32_t attack=game_read(game)->tower.attack, defence=game_read(game)->tower.defence;
    assert(game_read(game)->tower.gold>=TOWER_SHOP_COST);
    tap(game,240,240);
    assert(game_needs_redraw(game) && game_read(game)->tower.attack==attack+TOWER_SHOP_GAIN);
    assert(game_read(game)->gain_kind==GAME_GAIN_ATTACK);
    settle(game);
    for (int i=0;i<1000;++i) game_update(game);
    assert(game_read(game)->gain_kind==GAME_GAIN_ATTACK && game_is_idle(game));
    /* Restore the pre-shop checkpoint to exercise either purchase with the
     * same earned gold, without altering the model or its save format. */
    press(game,GAME_ACTION_LEFT); load_ok=true; tap(game,240,462);
    walking_press(game,GAME_ACTION_UP); settle(game);
    tap(game,240,282);
    assert(game_needs_redraw(game) && game_read(game)->tower.defence==defence+TOWER_SHOP_GAIN);
    assert(game_read(game)->gain_kind==GAME_GAIN_DEFENCE);
    settle(game);
    while (game_read(game)->tower.gold>=TOWER_SHOP_COST) {
        press(game,GAME_ACTION_START);
        assert(game_needs_redraw(game)); settle(game);
    }
    press(game,GAME_ACTION_START);
    assert(game_needs_redraw(game) && game_read(game)->tower.status==TOWER_STATUS_NEED_GOLD);
    assert(game_read(game)->gain_kind==GAME_GAIN_NONE);
    settle(game); press(game,GAME_ACTION_LEFT);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_DUNGEON);
    settle(game); walking_press(game,GAME_ACTION_DOWN); walking_press(game,GAME_ACTION_UP);
    assert(game_read(game)->screen==GAME_SHOP);
    settle(game); tap(game,240,340);
    assert(game_needs_redraw(game) && game_read(game)->screen==GAME_DUNGEON);
    game_delete(game); load_ok=false;
}
int main(void)
{
    assert(GAME_TICK_HZ==15 && GAME_KEY_REPEAT==4);
    test_pending_and_idle();
    test_sticky_feedback_and_storage();
    test_pointer_volume_pause_reset_and_dialog();
    test_held_repeat_and_combat();
    test_route_music();
    test_shop_transitions_and_feedback();
    puts("PASS: pending frames, settled idle, sticky feedback, operations, volume, audio, held combat and full replay");
    return 0;
}
