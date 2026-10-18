// SPDX-License-Identifier: MIT
/* Inert open-floor fixtures exercise the actual game and tower models. */
#include "game.h"
#include "game_config.h"
#include "tower_floors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char cells19[19][20], cells13[13][14], cells11[11][12];
static const char *rows19[19], *rows13[13], *rows11[11];
#define F19 {19u,19u,rows19}
#define F13 {13u,13u,rows13}
#define F11 {11u,11u,rows11}
const tower_floor_desc_t tower_floor_table[TOWER_FLOORS] = {
    F19,F13,F11,F19,F19,F19,F19,F19,F19,F19,
    F19,F19,F19,F19,F19,F19,F19,F19,F19,F19,
    F19,F19,F19,F19,F19,F19,F19,F19,F19,F19,
    F19,F19,F19,F19,F19,F19,F19,F19,F19,F19,
    F19,F19,F19,F19,F19,F19,F19,F19,F19,F19
};
static uint8_t saved[TOWER_SAVE_BYTES];
static int save_calls;
static bool have_save;
static bool save_bytes(void *ctx, const uint8_t *bytes, size_t count)
{
    (void)ctx;
    assert(count == sizeof saved);
    memcpy(saved, bytes, count);
    have_save = true;
    ++save_calls;
    return true;
}
static bool load_bytes(void *ctx, uint8_t *bytes, size_t count)
{
    (void)ctx;
    assert(count == sizeof saved);
    if (!have_save) return false;
    memcpy(bytes, saved, count);
    return true;
}
static void music(int track) { (void)track; }
static void fixture(unsigned sx, unsigned sy)
{
    for (int y=0; y<19; ++y) {
        memset(cells19[y], '.', 19); cells19[y][19]='\0'; rows19[y]=cells19[y];
    }
    for (int y=0; y<13; ++y) {
        memset(cells13[y], '.', 13); cells13[y][13]='\0'; rows13[y]=cells13[y];
    }
    for (int y=0; y<11; ++y) {
        memset(cells11[y], '.', 11); cells11[y][11]='\0'; rows11[y]=cells11[y];
    }
    cells19[sy][sx]='D'; cells19[17][17]='U';
    cells13[6][6]='D'; cells13[11][11]='U';
    cells11[5][5]='D'; cells11[9][9]='U';
    have_save=false; save_calls=0;
}
static void press(game_handle_t g, int action)
{ game_set_action(g,action,true); game_set_action(g,action,false); }
static void tap(game_handle_t g, int x, int y)
{ game_set_pointer(g,x,y,true); game_set_pointer(g,0,0,false); }
static void settle(game_handle_t g)
{ game_update(g); game_render_complete(g); assert(game_is_idle(g)); }
static game_handle_t create(void)
{
    const game_config_t cfg={.width=GAME_WIDTH,.height=GAME_HEIGHT,
        .save=save_bytes,.load=load_bytes,.set_music=music};
    game_handle_t g=NULL;
    assert(game_create(&cfg,&g) && g);
    return g;
}
static game_handle_t start(void)
{
    game_handle_t g=create();
    press(g,GAME_ACTION_START);
    assert(game_read(g)->screen==GAME_DUNGEON);
    assert(game_read(g)->has_checkpoint && save_calls==1);
    settle(g);
    return g;
}
static void camera(game_handle_t g, unsigned x, unsigned y)
{
    const game_snapshot_t *s=game_read(g);
    if (s->camera_x!=x || s->camera_y!=y)
        fprintf(stderr,"camera at hero (%u,%u): expected (%u,%u), got (%u,%u)\n",
            (unsigned)s->tower.x,(unsigned)s->tower.y,x,y,
            (unsigned)s->camera_x,(unsigned)s->camera_y);
    assert(s->camera_x==x && s->camera_y==y);
}
static void walk(game_handle_t g, int action, int count)
{ for (int i=0; i<count; ++i) press(g,action); }
static void test_eight_tile_pages_and_three_tile_overlap(void)
{
    fixture(1,1);
    game_handle_t g=start();
    walk(g,GAME_ACTION_RIGHT,8); camera(g,0,0);
    press(g,GAME_ACTION_RIGHT); camera(g,8,0);
    assert(game_read(g)->tower.x==10u);
    /* Old world columns 8,9,10 are the new page's first three columns. */
    assert(10u-game_read(g)->camera_x==2u);
    press(g,GAME_ACTION_LEFT); camera(g,8,0);
    press(g,GAME_ACTION_LEFT); camera(g,0,0);
    walk(g,GAME_ACTION_DOWN,8); camera(g,0,0);
    press(g,GAME_ACTION_DOWN); camera(g,0,8);
    walk(g,GAME_ACTION_UP,2); camera(g,0,0);
    assert(save_calls==1);
    assert(game_read(g)->tower.hp==100u && game_read(g)->tower.gold==0u);
    assert(game_read(g)->tower.turns==22u);
    settle(g);
    const uint32_t hash=game_hash(g), tick=game_read(g)->tick;
    for (int i=0; i<60; ++i) game_update(g);
    assert(game_hash(g)==hash && game_read(g)->tick==tick);
    assert(!game_needs_redraw(g) && game_is_idle(g));
    walk(g,GAME_ACTION_RIGHT,10);
    walk(g,GAME_ACTION_DOWN,10);
    assert(game_read(g)->tower.x==18u && game_read(g)->tower.y==18u);
    camera(g,8,8);
    press(g,GAME_ACTION_RIGHT); press(g,GAME_ACTION_DOWN);
    camera(g,8,8);
    assert(game_read(g)->tower.x==18u && game_read(g)->tower.y==18u);
    assert(save_calls==1);
    game_delete(g);
    puts("ok: movement pages by eight with three tiles overlap and settles idle");
}
static void test_center_new_restart_load_and_warp(void)
{
    fixture(9,9);
    cells19[17][17]='.'; cells19[9][10]='U';
    game_handle_t g=start();
    camera(g,4,4);
    press(g,GAME_ACTION_RIGHT);
    assert(game_read(g)->tower.floor==1u);
    camera(g,1,1);
    assert(save_calls==2);
    tap(g,187,462); /* warp -> anchor 1 */
    assert(game_read(g)->screen==GAME_TELEPORT);
    tap(g,98,172);
    assert(game_read(g)->tower.floor==0u);
    assert(game_read(g)->tower.x==9u && game_read(g)->tower.y==9u);
    camera(g,4,4);
    assert(save_calls==3);
    /* Restart must center from the spawn, irrespective of the former page. */
    walk(g,GAME_ACTION_DOWN,5); camera(g,4,8);
    press(g,GAME_ACTION_RESET);
    assert(game_read(g)->screen==GAME_NEW_CONFIRM);
    press(g,GAME_ACTION_START);
    camera(g,4,4);
    assert(save_calls==4);
    game_delete(g);
    g=create();
    assert(game_read(g)->has_save);
    tap(g,240,340);
    assert(game_read(g)->notice_id==GAME_NOTICE_RESTORED);
    camera(g,4,4);
    assert(save_calls==4);
    settle(g);
    game_delete(g);
    puts("ok: new run, restart, load, floor entry and warp center on the hero");
}
static void test_small_floor_clamp_and_single_viewport(void)
{
    fixture(1,1);
    cells19[17][17]='.'; cells19[1][2]='U';
    cells13[11][11]='.'; cells13[12][12]='U';
    game_handle_t g=start();
    press(g,GAME_ACTION_RIGHT);
    assert(game_read(g)->floor_w==13u && game_read(g)->floor_h==13u);
    camera(g,1,1);
    walk(g,GAME_ACTION_RIGHT,4); camera(g,1,1); /* x10: local9 */
    press(g,GAME_ACTION_RIGHT); camera(g,2,1); /* x11: clamped page */
    walk(g,GAME_ACTION_DOWN,4); camera(g,2,1);
    press(g,GAME_ACTION_DOWN); camera(g,2,2);
    press(g,GAME_ACTION_DOWN);
    press(g,GAME_ACTION_RIGHT); /* (12,12) stair -> the 11x11 floor */
    assert(game_read(g)->tower.floor==2u);
    assert(game_read(g)->floor_w==11u && game_read(g)->floor_h==11u);
    camera(g,0,0);
    walk(g,GAME_ACTION_RIGHT,5);
    walk(g,GAME_ACTION_DOWN,5);
    camera(g,0,0);
    assert(game_read(g)->tower.x==10u && game_read(g)->tower.y==10u);
    assert(save_calls==3); /* initial checkpoint plus the two floor changes */
    settle(g); game_delete(g);
    puts("ok: 13x13 pages clamp to two tiles; 11x11 floors never scroll");
}
static void test_refused_movement_and_combat(void)
{
    fixture(1,5);
    cells19[5][10]='#';
    game_handle_t g=start();
    walk(g,GAME_ACTION_RIGHT,8); camera(g,0,0);
    const uint32_t turns=game_read(g)->tower.turns;
    press(g,GAME_ACTION_RIGHT);
    assert(game_read(g)->tower.status==TOWER_STATUS_WALL);
    assert(game_read(g)->tower.x==9u && game_read(g)->tower.turns==turns);
    camera(g,0,0);
    /* A refused enemy, preview, and cancellation must all leave the page. */
    game_delete(g);
    fixture(1,5); cells19[5][10]='g';
    g=start(); walk(g,GAME_ACTION_RIGHT,8);
    press(g,GAME_ACTION_RIGHT);
    assert(game_read(g)->tower.status==TOWER_STATUS_TOO_STRONG);
    camera(g,0,0); assert(game_read(g)->tower.x==9u);
    game_delete(g);
    fixture(1,5); cells19[5][10]='s';
    g=start(); walk(g,GAME_ACTION_RIGHT,8);
    press(g,GAME_ACTION_RIGHT);
    assert(game_read(g)->tower.phase==TOWER_PHASE_COMBAT);
    camera(g,0,0); assert(game_read(g)->tower.x==9u);
    press(g,GAME_ACTION_LEFT);
    assert(game_read(g)->tower.phase==TOWER_PHASE_PLAY);
    camera(g,0,0); assert(game_read(g)->tower.x==9u);
    press(g,GAME_ACTION_RIGHT);
    tap(g,265,299); /* pointer confirm follows the same physical movement rule */
    assert(game_read(g)->tower.phase==TOWER_PHASE_PLAY);
    assert(game_read(g)->tower.x==10u);
    camera(g,8,0);
    assert(game_read(g)->tower.turns==9u);
    assert(save_calls==1);
    settle(g); game_delete(g);
    /* Exercise keyboard confirmation independently of pointer confirmation. */
    fixture(1,5); cells19[5][10]='s';
    g=start(); walk(g,GAME_ACTION_RIGHT,8);
    press(g,GAME_ACTION_RIGHT); press(g,GAME_ACTION_START);
    camera(g,8,0); assert(game_read(g)->tower.x==10u);
    assert(save_calls==1);
    game_delete(g);
    puts("ok: walls, refusal and combat cancel hold the page; winning combat pages");
}
static void test_edge_taps_swipes_and_gutters_are_not_camera_controls(void)
{
    fixture(1,5);
    game_handle_t g=start();
    walk(g,GAME_ACTION_RIGHT,8);
    /* This tile is under the former right camera-arrow hit rectangle. */
    tap(g,GAME_MAP_X+10*GAME_TILE+16,GAME_MAP_Y+5*GAME_TILE+16);
    assert(game_read(g)->tower.x==10u);
    camera(g,8,0);
    /* World-space mapping must include the new camera origin. */
    tap(g,GAME_MAP_X+3*GAME_TILE+16,GAME_MAP_Y+5*GAME_TILE+16);
    assert(game_read(g)->tower.x==11u); camera(g,8,0);
    /* A swipe beginning where the former arrow lived still moves once. */
    game_set_pointer(g,400,252,true);
    game_set_pointer(g,370,252,true);
    game_set_pointer(g,0,0,false);
    assert(game_read(g)->tower.x==10u); camera(g,8,0);
    const uint32_t turns=game_read(g)->tower.turns;
    settle(g);
    const uint32_t tick=game_read(g)->tick;
    game_set_pointer(g,400,252,true);
    game_render_complete(g);
    for (int i=0; i<20; ++i) game_update(g);
    assert(game_is_idle(g) && !game_needs_redraw(g));
    assert(game_read(g)->tick==tick);
    game_set_pointer(g,0,0,false);
    assert(game_read(g)->tower.turns==turns);
    /* Former proposed gutter controls have no independent camera behavior. */
    const int gutter[][2]={{10,174},{10,270},{426,174},{426,270},{426,366}};
    for (unsigned i=0; i<sizeof gutter/sizeof gutter[0]; ++i) {
        game_set_pointer(g,gutter[i][0]+10,gutter[i][1]+10,true);
        for (int tick=0; tick<20; ++tick) game_update(g);
        game_set_pointer(g,0,0,false);
        camera(g,8,0);
        assert(game_read(g)->tower.turns==turns && game_read(g)->tower.x==10u);
    }
    assert(save_calls==1);
    settle(g); game_delete(g);
    puts("ok: translucent edges preserve tap/swipe movement; gutters never pan");
}
int main(void)
{
    test_eight_tile_pages_and_three_tile_overlap();
    test_center_new_restart_load_and_warp();
    test_small_floor_clamp_and_single_viewport();
    test_refused_movement_and_combat();
    test_edge_taps_swipes_and_gutters_are_not_camera_controls();
    puts("PASS: automatic viewport edge paging");
    return 0;
}
