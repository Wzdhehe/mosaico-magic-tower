// SPDX-License-Identifier: MIT
// Portable scheduling and input regression coverage; no Mosaico BSP required.
#include "game.h"
#include "game_config.h"
#include "win_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int music_calls, last_music, volume_calls, brightness_calls, last_volume, last_brightness;
static bool brightness_ok=true, preferences_load_ok, preferences_save_ok=true;
static int preferences_load_calls, preferences_save_calls;
static game_preferences_t stored_preferences;
static int preferences_context;
static bool brightness(int value) { last_brightness=value; ++brightness_calls; return brightness_ok; }
static bool load_preferences(void *context, game_preferences_t *out)
{
    assert(context==&preferences_context); ++preferences_load_calls;
    if (!preferences_load_ok) return false;
    *out=stored_preferences; return true;
}
static bool save_preferences(void *context, const game_preferences_t *value)
{
    assert(context==&preferences_context); ++preferences_save_calls;
    if (!preferences_save_ok) return false;
    stored_preferences=*value; return true;
}
static bool save_ok, load_ok;
static uint8_t saved[TOWER_SAVE_BYTES];
static void music(int track) { ++music_calls; last_music=track; }
static void volume(int value) { last_volume=value; ++volume_calls; }
static bool save_bytes(void *context, const uint8_t *bytes, size_t count)
{ (void)context; if (!save_ok) return false; memcpy(saved,bytes,count); return true; }
static bool load_bytes(void *context, uint8_t *bytes, size_t count)
{ (void)context; if (!load_ok) return false; memcpy(bytes,saved,count); return true; }
static game_handle_t new_game(void)
{
    const game_config_t cfg = {.width=GAME_WIDTH, .height=GAME_HEIGHT,
        .save=save_bytes,.load=load_bytes,.set_music=music,.set_volume=volume,
        .set_brightness=brightness,.load_preferences=load_preferences,
        .save_preferences=save_preferences,.preferences_context=&preferences_context};
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
static void test_gear_opens_without_changing_language(void)
{
    game_handle_t game=new_game();
    tap(game,437,30);
    assert(game_read(game)->lang==GAME_LANG_ZH && game_read(game)->settings_open);
    settle(game);
    tap(game,250,158); /* Selecting the current language is a settled no-op. */
    assert(!game_needs_redraw(game) && game_is_idle(game));
    tap(game,240,388); /* Repository address is information, without navigation state. */
    assert(game_read(game)->settings_open && !game_needs_redraw(game));
    tap(game,390,102); settle(game);
    tap(game,50,30); /* Removed speaker button no longer handles input. */
    assert(!game_read(game)->settings_open && game_is_idle(game));
    game_delete(game);
}
static void test_gear_expanded_touch_target(void)
{
    const int opens[][2]={
        {413,36}, {464,36}, {438,13}, {438,58}, /* Outside visible edges. */
        {390,8}, {465,59}, /* Inclusive corners of the original language hitbox. */
        {390,30}, {430,59} /* Inclusive expanded left and bottom edges. */
    };
    int missed=0;
    for (unsigned i=0;i<sizeof(opens)/sizeof(opens[0]);++i) {
        game_handle_t game=new_game();
        tap(game,opens[i][0],opens[i][1]);
        if (!game_read(game)->settings_open) {
            fprintf(stderr,"Gear missed expanded touch at %d,%d\n",opens[i][0],opens[i][1]);
            ++missed;
        }
        assert(game_read(game)->screen==GAME_TITLE && game_read(game)->lang==GAME_LANG_ZH);
        game_delete(game);
    }
    assert(missed==0);
    const int outside[][2]={{389,30},{466,30},{430,7},{430,60},{479,30},{430,0},{50,30}};
    for (unsigned i=0;i<sizeof(outside)/sizeof(outside[0]);++i) {
        game_handle_t game=new_game(); settle(game);
        tap(game,outside[i][0],outside[i][1]);
        assert(!game_read(game)->settings_open && !game_needs_redraw(game));
        game_delete(game);
    }
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
    assert(game_read(game)->gain_kind==GAME_GAIN_NONE);
    press(game,GAME_ACTION_UP); press(game,GAME_ACTION_UP);
    press(game,GAME_ACTION_UP); press(game,GAME_ACTION_LEFT);
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
static void test_pointer_settings_pause_reset_and_dialog(void)
{
    game_handle_t game=new_started_game();
    game_set_pointer(game,240,350,true);
    for (int i=0;i<10;++i) game_set_pointer(game,240,350,true);
    assert(game_is_idle(game) && !game_needs_redraw(game));
    game_set_pointer(game,0,0,false);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==8);
    settle(game);
    tap(game,437,30);
    assert(game_needs_redraw(game) && game_read(game)->settings_open);
    settle(game);
    tap(game,350,158);
    assert(game_needs_redraw(game) && game_read(game)->lang==GAME_LANG_EN);
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
    tap(game,390,102);
    assert(game_needs_redraw(game) && !game_read(game)->settings_open);
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
static void test_preferences_validation_persistence_and_failure(void)
{
    const game_preferences_t invalid[]={{2,45,40},{0,101,40},{0,45,9},{0,45,101}};
    preferences_load_ok=true;
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        stored_preferences=invalid[i];
        game_handle_t bad=new_game();
        assert(game_read(bad)->lang==GAME_LANG_ZH && game_read(bad)->volume==45);
        assert(game_read(bad)->brightness==GAME_BRIGHTNESS_DEFAULT);
        game_delete(bad);
    }
    stored_preferences=(game_preferences_t){GAME_LANG_EN,0,10};
    const int vc=volume_calls, bc=brightness_calls, lc=preferences_load_calls;
    game_handle_t game=new_game();
    assert(preferences_load_calls==lc+1 && volume_calls==vc && brightness_calls==bc);
    assert(game_read(game)->lang==GAME_LANG_EN && game_read(game)->volume==0 && game_read(game)->brightness==10);
    press(game,GAME_ACTION_START); settle(game);
    save_ok=true; tap(game,135,462);
    uint8_t journey_before[TOWER_SAVE_BYTES]; memcpy(journey_before,saved,sizeof(saved));
    const int sc=preferences_save_calls;
    tap(game,437,30); tap(game,250,158);
    game_set_pointer(game,80,230,true);
    game_set_pointer(game,400,230,true);
    game_set_pointer(game,0,0,false);
    assert(game_read(game)->volume==100 && last_volume==100 && preferences_save_calls==sc);
    game_set_pointer(game,80,306,true);
    game_set_pointer(game,240,306,true);
    game_set_pointer(game,400,306,true);
    assert(brightness_calls==bc && game_read(game)->brightness==10);
    assert(!game_is_idle(game));
    game_update(game);
    assert(brightness_calls==bc+1 && last_brightness==100 && game_read(game)->brightness==100);
    game_render_complete(game);
    for (int i=0;i<20;++i) game_set_pointer(game,400,306,true);
    game_update(game);
    assert(brightness_calls==bc+1 && !game_needs_redraw(game));
    game_set_pointer(game,0,0,false);
    tap(game,390,102);
    assert(preferences_save_calls==sc+1 && !game_read(game)->preferences_save_failed);
    assert(!memcmp(saved,journey_before,sizeof(saved)));
    game_reset(game); tap(game,285,280);
    assert(game_read(game)->lang==GAME_LANG_ZH && game_read(game)->volume==100 && game_read(game)->brightness==100);
    load_ok=true; tap(game,240,462); load_ok=false;
    assert(game_read(game)->lang==GAME_LANG_ZH && game_read(game)->volume==100 && game_read(game)->brightness==100);
    game_delete(game); game=new_game();
    assert(game_read(game)->lang==GAME_LANG_ZH && game_read(game)->volume==100 && game_read(game)->brightness==100);
    tap(game,437,30); tap(game,390,102);
    assert(preferences_save_calls==sc+1);
    tap(game,437,30);
    brightness_ok=false;
    game_set_pointer(game,80,306,true); game_update(game);
    assert(game_read(game)->brightness==100 && brightness_calls==bc+2);
    for (int i=0;i<20;++i) { game_set_pointer(game,80,306,true); game_update(game); }
    assert(brightness_calls==bc+2 && game_read(game)->brightness==100);
    game_set_pointer(game,0,0,false);
    tap(game,390,102);
    assert(preferences_save_calls==sc+1);
    brightness_ok=true; tap(game,437,30); tap(game,80,306);
    preferences_save_ok=false; tap(game,390,102);
    assert(game_read(game)->brightness==10 && last_brightness==10 && brightness_calls==bc+3);
    assert(preferences_save_calls==sc+2 && game_read(game)->preferences_save_failed);
    settle(game);
    for (int i=0;i<100;++i) game_update(game);
    assert(preferences_save_calls==sc+2 && game_is_idle(game));
    preferences_save_ok=true; tap(game,437,30); tap(game,390,102);
    assert(preferences_save_calls==sc+3 && !game_read(game)->preferences_save_failed);
    game_delete(game); preferences_load_ok=false;
}
static void test_settings_capture_and_held_suppression(void)
{
    game_handle_t game=new_started_game();
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_read(game)->tower.y==8);
    tap(game,437,30); settle(game);
    const uint32_t turns=game_read(game)->tower.turns;
    for (int i=0;i<20;++i) { game_set_action(game,GAME_ACTION_UP,true); game_update(game); }
    assert(game_read(game)->settings_open && game_read(game)->tower.turns==turns);
    press(game,GAME_ACTION_RESET); press(game,GAME_ACTION_PAUSE);
    assert(game_read(game)->screen==GAME_DUNGEON && !game_read(game)->paused);
    /* Slider capture cannot close the panel or move when released outside it. */
    game_set_pointer(game,200,230,true); game_set_pointer(game,450,450,true);
    game_set_pointer(game,0,0,false);
    assert(game_read(game)->settings_open && game_read(game)->tower.turns==turns);
    /* A swipe that begins in panel space cannot reach the map underneath. */
    game_set_pointer(game,240,350,true); game_set_pointer(game,240,410,true);
    game_set_pointer(game,0,0,false);
    assert(game_read(game)->tower.turns==turns);
    tap(game,390,102); settle(game);
    for (int i=0;i<20;++i) { game_set_action(game,GAME_ACTION_UP,true); game_update(game); }
    assert(game_read(game)->tower.turns==turns && game_is_idle(game));
    game_set_action(game,GAME_ACTION_UP,false); press(game,GAME_ACTION_UP);
    assert(game_read(game)->tower.y==7);
    game_delete(game);
    game=new_started_game();
    press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT);
    press(game,GAME_ACTION_UP); assert(game_read(game)->tower.phase==TOWER_PHASE_COMBAT);
    tap(game,437,30);
    tap(game,285,298); /* brightness track covers combat confirm */
    game_update(game); tap(game,390,102);
    assert(game_read(game)->tower.phase==TOWER_PHASE_COMBAT && game_read(game)->tower.y==9);
    tap(game,437,30); press(game,GAME_ACTION_LEFT);
    assert(!game_read(game)->settings_open && game_read(game)->tower.phase==TOWER_PHASE_COMBAT);
    game_delete(game);
}
static void test_settings_hold_non_direction_actions_until_release(void)
{
    const int codes[]={GAME_ACTION_RESET,GAME_ACTION_PAUSE,GAME_ACTION_START};
    int leaked=0;
    for (unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        const int code=codes[i];
        game_handle_t game=code==GAME_ACTION_START ? new_game() : new_started_game();
        const game_screen_t original=game_read(game)->screen;
        tap(game,437,30);
        game_set_action(game,code,true);
        if (code==GAME_ACTION_START) {
            assert(!game_read(game)->settings_open);
            tap(game,437,30);
        }
        assert(game_read(game)->settings_open);
        /* Both settings slider capture and the closing X must preserve the
         * physical key's held state when no release sample has arrived. */
        game_set_pointer(game,240,230,true);
        game_set_pointer(game,300,230,true);
        game_set_pointer(game,0,0,false);
        tap(game,390,102); settle(game);
        game_set_action(game,code,true);
        for (int n=0;n<10;++n) game_update(game);
        if (game_read(game)->screen!=original || game_read(game)->paused ||
            game_needs_redraw(game)) {
            fprintf(stderr,"Settings leaked held action %d after pointer close\n",code);
            ++leaked;
        }
        game_delete(game);
    }
    assert(leaked==0);
    /* The release must rearm the same controls, rather than disable them. */
    for (unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        const int code=codes[i];
        game_handle_t game=code==GAME_ACTION_START ? new_game() : new_started_game();
        tap(game,437,30);
        game_set_action(game,code,true);
        if (code==GAME_ACTION_START) tap(game,437,30);
        tap(game,390,102);
        game_set_action(game,code,false); press(game,code);
        if (code==GAME_ACTION_RESET) assert(game_read(game)->screen==GAME_NEW_CONFIRM);
        else if (code==GAME_ACTION_PAUSE) assert(game_read(game)->paused);
        else assert(game_read(game)->screen==GAME_DUNGEON);
        game_delete(game);
    }
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
    press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT);
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_needs_redraw(game) && game_read(game)->tower.phase==TOWER_PHASE_COMBAT);
    settle(game);
    press(game,GAME_ACTION_START);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==8);
    settle(game);
    const uint32_t turns=game_read(game)->tower.turns;
    for (int i=0;i<100;++i) { game_set_action(game,GAME_ACTION_UP,true); game_update(game); }
    assert(game_is_idle(game) && !game_needs_redraw(game) && game_read(game)->tower.turns==turns);
    game_set_action(game,GAME_ACTION_UP,false); press(game,GAME_ACTION_UP);
    assert(game_needs_redraw(game) && game_read(game)->tower.y==7);
    game_delete(game);
    game=new_started_game();
    press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT); press(game,GAME_ACTION_LEFT);
    press(game,GAME_ACTION_UP); settle(game);
    tap(game,180,298);
    assert(game_needs_redraw(game) && game_read(game)->tower.phase==TOWER_PHASE_PLAY);
    assert(game_read(game)->tower.y==9);
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
    /* Walk the right-hand aisle to the merchant; save one tile below it. */
    for (int i=0;i<3;++i) walking_press(game,GAME_ACTION_RIGHT);
    walking_press(game,GAME_ACTION_UP);
    walking_press(game,GAME_ACTION_RIGHT);
    walking_press(game,GAME_ACTION_UP);
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
    while (game_read(game)->tower.gold>=
           tower_shop_price(&game_read(game)->tower)) {
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
    test_gear_opens_without_changing_language();
    test_gear_expanded_touch_target();
    test_pending_and_idle();
    test_sticky_feedback_and_storage();
    test_pointer_settings_pause_reset_and_dialog();
    test_preferences_validation_persistence_and_failure();
    test_settings_capture_and_held_suppression();
    test_settings_hold_non_direction_actions_until_release();
    test_held_repeat_and_combat();
    test_route_music();
    test_shop_transitions_and_feedback();
    puts("PASS: pending frames, settled idle, sticky feedback, operations, settings, preferences, brightness, audio, held combat and full replay");
    return 0;
}
