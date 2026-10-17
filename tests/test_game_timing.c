// SPDX-License-Identifier: MIT
// Portable timing and input regression coverage; no Mosaico BSP required.
#include "game.h"
#include "game_config.h"
#include <assert.h>
#include <stdio.h>

static game_handle_t new_started_game(void)
{
    const game_config_t cfg = {.width=GAME_WIDTH, .height=GAME_HEIGHT};
    game_handle_t game=NULL;
    assert(game_create(&cfg,&game));
    assert(game != NULL);
    game_set_action(game,GAME_ACTION_START,true);
    game_set_action(game,GAME_ACTION_START,false);
    assert(game_read(game)->screen==GAME_DUNGEON);
    return game;
}

int main(void)
{
    /* Regression guard: keep the puzzle game well below its former 30 FPS. */
    assert(GAME_TICK_HZ==15);
    assert(GAME_NOTICE_TICKS==45);
    assert(GAME_GAIN_TICKS==60);
    assert(GAME_KEY_REPEAT==4);

    game_handle_t game=new_started_game();
    assert(game_read(game)->notice_id==GAME_NOTICE_START);
    for (int i=0;i<GAME_NOTICE_TICKS-1;++i) game_update(game);
    assert(game_read(game)->notice_id==GAME_NOTICE_START);
    game_update(game);
    assert(game_read(game)->notice_id==GAME_NOTICE_NONE);

    game_set_action(game,GAME_ACTION_UP,true);
    game_set_action(game,GAME_ACTION_UP,false);
    assert(game_read(game)->tower.y==8);
    assert(game_read(game)->gain_kind==GAME_GAIN_HP);
    for (int i=0;i<GAME_GAIN_TICKS-1;++i) game_update(game);
    assert(game_read(game)->gain_kind==GAME_GAIN_HP);
    game_update(game);
    assert(game_read(game)->gain_kind==GAME_GAIN_NONE);
    game_delete(game);

    game=new_started_game();
    game_set_action(game,GAME_ACTION_UP,true);
    assert(game_read(game)->tower.y==8);
    for(int i=0;i<GAME_KEY_REPEAT-1;++i) game_update(game);
    assert(game_read(game)->tower.y==8);
    game_update(game);
    assert(game_read(game)->tower.y==7);
    game_set_action(game,GAME_ACTION_UP,false);
    for(int i=0;i<GAME_KEY_REPEAT*2;++i) game_update(game);
    assert(game_read(game)->tower.y==7);
    game_delete(game);

    puts("PASS: low-FPS timing, notifications, pickup feedback, key repeat");
    return 0;
}
