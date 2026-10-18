// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
struct game_t {
    game_config_t config;
    game_snapshot_t state;
    tower_handle_t model;
    uint8_t save_bytes[TOWER_SAVE_BYTES];
    int32_t press_x, press_y;
    bool held[7];
    bool suppressed[7];
    bool frame_pending;
    uint8_t repeat;
    game_screen_t press_screen;
    tower_phase_t press_phase;
};
static void refresh(game_handle_t g) { (void)tower_read(g->model, &g->state.tower); }
static void set_screen(game_handle_t g, game_screen_t screen)
{
    if (g->state.screen != screen) { g->state.screen=screen; g->frame_pending=true; }
}
static void set_volume_open(game_handle_t g, bool open)
{
    if (g->state.volume_open != open) { g->state.volume_open=open; g->frame_pending=true; }
}
static void suppress_directions(game_handle_t g)
{
    for (int code=0; code<7; ++code)
        if (code==GAME_ACTION_LEFT || code==GAME_ACTION_RIGHT || code==GAME_ACTION_UP || code==GAME_ACTION_DOWN)
            if (g->held[code]) g->suppressed[code]=true;
    g->repeat=0;
}
static void clear_gain(game_handle_t g)
{
    g->state.gain_kind = GAME_GAIN_NONE;
    g->state.gain_value = 0;
    g->state.gain_color = 0;
}
static void clear_feedback(game_handle_t g)
{
    g->state.notice_id=GAME_NOTICE_NONE;
    clear_gain(g);
}
static void notice(game_handle_t g, game_notice_t id)
{
    clear_gain(g);
    g->state.notice_id = (uint8_t)id;
    g->frame_pending = true;
}
static void set_gain(game_handle_t g, game_gain_t kind, uint32_t value, uint8_t color)
{
    g->state.gain_kind = (uint8_t)kind;
    g->state.gain_value = value;
    g->state.gain_color = color;
    g->state.notice_id = GAME_NOTICE_NONE;
    g->frame_pending = true;
}
static void start(game_handle_t g)
{
    tower_new(g->model);
    g->state.screen = GAME_DUNGEON;
    g->state.paused = false;
    g->state.volume_open = false;
    clear_gain(g);
    notice(g, GAME_NOTICE_START);
    refresh(g);
    suppress_directions(g);
}
static bool load(game_handle_t g)
{
    if (!g->config.load || !g->config.load(g->config.storage_context, g->save_bytes, sizeof(g->save_bytes))) {
        notice(g, GAME_NOTICE_NO_SAVE); return false;
    }
    if (tower_import(g->model, g->save_bytes, sizeof(g->save_bytes)) != TOWER_OK) {
        notice(g, GAME_NOTICE_DAMAGED); return false;
    }
    g->state.has_save = true;
    g->state.screen = GAME_DUNGEON;
    g->state.volume_open = false;
    clear_gain(g);
    notice(g, GAME_NOTICE_RESTORED);
    refresh(g);
    return true;
}
static void save(game_handle_t g)
{
    if (tower_export(g->model, g->save_bytes, sizeof(g->save_bytes)) != TOWER_OK || !g->config.save ||
        !g->config.save(g->config.storage_context, g->save_bytes, sizeof(g->save_bytes))) {
        notice(g, GAME_NOTICE_SAVE_FAILED); return;
    }
    g->state.has_save = true;
    notice(g, GAME_NOTICE_SAVED);
}
static void move(game_handle_t g, int dx, int dy)
{
    if (g->state.paused || g->state.screen != GAME_DUNGEON) return;
    const tower_tile_t target = game_tile(g, (int)g->state.tower.x + dx,
                                          (int)g->state.tower.y + dy);
    const tower_status_t result = tower_move(g->model, dx, dy);
    clear_feedback(g);
    g->frame_pending = true;
    refresh(g);
    if (result == TOWER_STATUS_ITEM) {
        switch (target) {
        case TOWER_TILE_POTION: set_gain(g, GAME_GAIN_HP, 30, 0); break;
        case TOWER_TILE_BIG_POTION: set_gain(g, GAME_GAIN_HP, 80, 0); break;
        case TOWER_TILE_ATTACK: set_gain(g, GAME_GAIN_ATTACK, 5, 0); break;
        case TOWER_TILE_DEFENCE: set_gain(g, GAME_GAIN_DEFENCE, 3, 0); break;
        case TOWER_TILE_KEY_YELLOW: set_gain(g, GAME_GAIN_KEY, 1, 0); break;
        case TOWER_TILE_KEY_BLUE: set_gain(g, GAME_GAIN_KEY, 1, 1); break;
        case TOWER_TILE_KEY_RED: set_gain(g, GAME_GAIN_KEY, 1, 2); break;
        default: break;
        }
    }
    if (g->state.tower.status == TOWER_STATUS_SHOP) g->state.screen = GAME_SHOP;
    if (g->state.tower.status == TOWER_STATUS_TALK) g->state.screen = GAME_DIALOG;
    if (g->state.tower.phase==TOWER_PHASE_COMBAT) suppress_directions(g);
    if (g->state.screen == GAME_DIALOG) suppress_directions(g);
}
static bool inside(int x, int y, int bx, int by, int w, int h)
{ return x >= bx && x < bx + w && y >= by && y < by + h; }
static void set_volume(game_handle_t g, int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (g->state.volume == volume) return;
    g->state.volume = (uint8_t)volume;
    g->frame_pending = true;
    if (g->config.set_volume) g->config.set_volume(volume);
}
static int volume_from_x(int x)
{
    return (x - GAME_VOL_SLIDER_X) * 100 / GAME_VOL_SLIDER_W;
}
static bool in_volume_slider(int x, int y)
{
    return inside(x,y,GAME_VOL_SLIDER_X,GAME_VOL_SLIDER_Y-18,
                  GAME_VOL_SLIDER_W,GAME_VOL_SLIDER_H+36);
}
bool game_create(const game_config_t *config, game_handle_t *out)
{
    if (!out) return false;
    *out = NULL;
    if (!config || config->width != GAME_WIDTH || config->height != GAME_HEIGHT) return false;
    game_handle_t g = calloc(1, sizeof(*g));
    if (!g) return false;
    const tower_config_t model_config = {0};
    if (tower_create(&model_config, &g->model) != TOWER_OK) { free(g); return false; }
    g->config = *config;
    g->frame_pending = true;
    g->state.screen = GAME_TITLE;
    g->state.lang = GAME_LANG_ZH;
    g->state.volume = 45;
    g->state.music = 0;
    if (config->load && config->load(config->storage_context, g->save_bytes, sizeof(g->save_bytes))) {
        g->state.has_save = tower_import(g->model, g->save_bytes, sizeof(g->save_bytes)) == TOWER_OK;
        tower_new(g->model);
    }
    refresh(g);
    *out = g;
    return true;
}
void game_delete(game_handle_t g) { if (g) { tower_delete(g->model); free(g); } }
void game_reset(game_handle_t g)
{
    if (!g) return;
    suppress_directions(g);
    clear_feedback(g);
    g->frame_pending = true;
    g->state.volume_open = false;
    if (g->state.screen == GAME_TITLE) start(g);
    else g->state.screen = GAME_NEW_CONFIRM;
}
void game_set_paused(game_handle_t g, bool paused)
{
    if (g) {
        suppress_directions(g);
        if (g->state.paused != paused) { g->state.paused=paused; g->frame_pending=true; }
    }
}
static void tap(game_handle_t g, int x, int y)
{
    if (g->press_screen != g->state.screen) return;
    if (g->state.volume_open) {
        if (in_volume_slider(x,y))
            set_volume(g,volume_from_x(x));
        else if (inside(x,y,GAME_VOL_CLOSE_X,GAME_VOL_CLOSE_Y,GAME_VOL_CLOSE_W,GAME_VOL_CLOSE_H))
            set_volume_open(g,false);
        else if (!inside(x,y,GAME_VOL_PANEL_X,GAME_VOL_PANEL_Y,GAME_VOL_PANEL_W,GAME_VOL_PANEL_H))
            set_volume_open(g,false);
        return;
    }
    if (inside(x,y,GAME_VOL_X-4,GAME_VOL_Y-4,GAME_VOL_W+8,GAME_VOL_H+8)) {
        set_volume_open(g,true);
        return;
    }
    if (inside(x,y,GAME_LANG_X-6,GAME_LANG_Y-6,GAME_LANG_W+12,GAME_LANG_H+12)) {
        g->state.lang = g->state.lang == GAME_LANG_ZH ? GAME_LANG_EN : GAME_LANG_ZH;
        g->frame_pending = true;
        return;
    }
    if (g->state.screen == GAME_TITLE) {
        if (inside(x,y,140,280,200,38)) start(g);
        else if (inside(x,y,140,326,200,34)) (void)load(g);
        return;
    }
    if (g->state.screen == GAME_DIALOG) {
        clear_feedback(g);
        set_screen(g,GAME_DUNGEON);
        return;
    }
    if (g->state.screen == GAME_NEW_CONFIRM) {
        if (inside(x,y,135,265,100,36)) set_screen(g,GAME_DUNGEON);
        else if (inside(x,y,245,265,100,36)) start(g);
        return;
    }
    if (g->state.screen == GAME_SHOP) {
        if (inside(x,y,130,222,220,36)) {
            clear_feedback(g);
            g->frame_pending=true;
            if (tower_buy(g->model,0) == TOWER_STATUS_ITEM) set_gain(g,GAME_GAIN_ATTACK,TOWER_SHOP_GAIN,0);
            refresh(g);
        }
        else if (inside(x,y,130,264,220,36)) {
            clear_feedback(g);
            g->frame_pending=true;
            if (tower_buy(g->model,1) == TOWER_STATUS_ITEM) set_gain(g,GAME_GAIN_DEFENCE,TOWER_SHOP_GAIN,0);
            refresh(g);
        }
        else if (inside(x,y,130,326,220,28)) { clear_feedback(g); set_screen(g,GAME_DUNGEON); }
        return;
    }
    if (g->state.tower.phase == TOWER_PHASE_COMBAT) {
        if (g->press_phase != TOWER_PHASE_COMBAT) return;
        if (inside(x,y,145,281,90,36)) (void)tower_cancel(g->model);
        else if (inside(x,y,245,281,90,36)) (void)tower_confirm(g->model);
        else return;
        clear_feedback(g);
        g->frame_pending=true;
        suppress_directions(g);
        refresh(g);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const int bx = GAME_BUTTON_X + i * (GAME_BUTTON_W + GAME_BUTTON_GAP);
        if (!inside(x,y,bx,GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H)) continue;
        if (i == 0) save(g); else if (i == 1) (void)load(g); else game_reset(g);
        return;
    }
    if (inside(x,y,GAME_MAP_X,GAME_MAP_Y,GAME_TILE*TOWER_WIDTH,GAME_TILE*TOWER_HEIGHT)) {
        const int dx = (x-GAME_MAP_X)/GAME_TILE - (int)g->state.tower.x;
        const int dy = (y-GAME_MAP_Y)/GAME_TILE - (int)g->state.tower.y;
        if (abs(dx)+abs(dy) == 1) move(g,dx,dy);
    }
}
void game_set_pointer(game_handle_t g, int32_t x, int32_t y, bool pressed)
{
    if (!g) return;
    const bool was_down = g->state.pointer_down;
    /* The single-point engine reports release as (0,0), not the last contact
     * position. Keep the last pressed coordinate so a tap is not mistaken for
     * a swipe to the top-left corner. */
    int32_t release_x = x;
    int32_t release_y = y;
    if (!pressed && x == 0 && y == 0) {
        release_x = g->state.pointer_x;
        release_y = g->state.pointer_y;
    }
    if (pressed && !was_down) {
        g->press_x=x; g->press_y=y;
        g->press_screen=g->state.screen; g->press_phase=g->state.tower.phase;
        memset(g->held,0,sizeof(g->held));
    }
    g->state.pointer_x=x; g->state.pointer_y=y; g->state.pointer_down=pressed;
    if (pressed && g->state.volume_open && in_volume_slider(x,y))
        set_volume(g,volume_from_x(x));
    if (pressed || !was_down || g->state.paused) return;
    const int dx=release_x-g->press_x, dy=release_y-g->press_y;
    if (abs(dx)>=GAME_SWIPE_MIN || abs(dy)>=GAME_SWIPE_MIN) {
        if (g->press_screen==GAME_DUNGEON && g->state.screen==GAME_DUNGEON &&
            g->press_phase!=TOWER_PHASE_COMBAT &&
            inside(g->press_x,g->press_y,GAME_MAP_X,GAME_MAP_Y,GAME_TILE*TOWER_WIDTH,GAME_TILE*TOWER_HEIGHT)) {
            if (abs(dx)>=abs(dy)) move(g,dx>0?1:-1,0); else move(g,0,dy>0?1:-1);
        }
    } else if (abs(dx)<10 && abs(dy)<10) tap(g,release_x,release_y);
}
static void action(game_handle_t g, int code)
{
    if (code==GAME_ACTION_PAUSE) { game_set_paused(g,!g->state.paused); return; }
    if (g->state.paused) return;
    if (g->state.volume_open) {
        if (code==GAME_ACTION_START || code==GAME_ACTION_LEFT) set_volume_open(g,false);
        return;
    }
    if (code==GAME_ACTION_RESET) { game_reset(g); return; }
    if (g->state.screen==GAME_TITLE) { start(g); return; }
    if (g->state.screen==GAME_DIALOG) {
        if (code==GAME_ACTION_START || code==GAME_ACTION_LEFT) { clear_feedback(g); set_screen(g,GAME_DUNGEON); }
        return;
    }
    if (g->state.screen==GAME_NEW_CONFIRM) {
        if (code==GAME_ACTION_START) start(g);
        else if (code==GAME_ACTION_LEFT) set_screen(g,GAME_DUNGEON);
        return;
    }
    if (g->state.screen==GAME_SHOP) {
        if (code==GAME_ACTION_START) {
            clear_feedback(g);
            g->frame_pending=true;
            if (tower_buy(g->model,0) == TOWER_STATUS_ITEM) set_gain(g,GAME_GAIN_ATTACK,TOWER_SHOP_GAIN,0);
        }
        else if (code==GAME_ACTION_LEFT) { clear_feedback(g); set_screen(g,GAME_DUNGEON); }
        refresh(g);
        return;
    }
    if (g->state.tower.phase==TOWER_PHASE_COMBAT) {
        if (code==GAME_ACTION_START) (void)tower_confirm(g->model);
        else if (code==GAME_ACTION_LEFT) (void)tower_cancel(g->model);
        else return;
        clear_feedback(g);
        g->frame_pending=true;
        suppress_directions(g);
        refresh(g); return;
    }
    if (code==GAME_ACTION_LEFT) move(g,-1,0);
    else if (code==GAME_ACTION_RIGHT) move(g,1,0);
    else if (code==GAME_ACTION_UP) move(g,0,-1);
    else if (code==GAME_ACTION_DOWN) move(g,0,1);
}
void game_set_action(game_handle_t g, int code, bool pressed)
{
    if (!g || code<0 || code>=7) return;
    if (g->suppressed[code]) {
        if (!pressed) { g->suppressed[code]=false; g->held[code]=false; }
        return;
    }
    if (g->state.pointer_down) { g->held[code]=false; return; }
    const bool rising=pressed && !g->held[code];
    g->held[code]=pressed;
    if (rising) { g->repeat=0; action(g,code); }
}
static uint8_t expected_music(game_handle_t g)
{ return g->state.screen==GAME_TITLE ? 0 : (g->state.tower.floor>=5 ? 2 : 1); }
static bool repeating_direction(game_handle_t g)
{
    if (g->state.paused || g->state.pointer_down || g->state.volume_open ||
        g->state.screen!=GAME_DUNGEON || g->state.tower.phase!=TOWER_PHASE_PLAY) return false;
    for (int code=0; code<=GAME_ACTION_DOWN; ++code)
        if (code!=GAME_ACTION_PAUSE && code!=GAME_ACTION_RESET && g->held[code] && !g->suppressed[code])
            return true;
    return false;
}
bool game_needs_redraw(game_handle_t g) { return g && g->frame_pending; }
void game_render_complete(game_handle_t g) { if (g) g->frame_pending=false; }
void game_request_redraw(game_handle_t g) { if (g) g->frame_pending=true; }
bool game_is_idle(game_handle_t g)
{ return !g || (!g->frame_pending && expected_music(g)==g->state.music && !repeating_direction(g)); }
void game_update(game_handle_t g)
{
    if (!g) return;
    const uint8_t music = expected_music(g);
    if (music!=g->state.music) {
        g->state.music=music;
        if (g->config.set_music) g->config.set_music(music);
    }
    if (!repeating_direction(g)) return;
    ++g->state.tick;
    if (++g->repeat>=GAME_KEY_REPEAT) {
        g->repeat=0;
        if (!g->state.pointer_down && g->state.screen==GAME_DUNGEON && g->state.tower.phase==TOWER_PHASE_PLAY)
            for (int code=0; code<=GAME_ACTION_DOWN; ++code)
                if (code!=GAME_ACTION_PAUSE && code!=GAME_ACTION_RESET && g->held[code] && !g->suppressed[code]) {
                    action(g,code); break;
                }
    }
}
const game_snapshot_t *game_read(game_handle_t g) { return g?&g->state:NULL; }
tower_tile_t game_tile(game_handle_t g,int x,int y)
{ return g?tower_tile(g->model,g->state.tower.floor,x,y):TOWER_TILE_WALL; }
uint32_t game_hash(game_handle_t g)
{
    if (!g) return 0;
    uint32_t h=tower_hash(g->model);
    h=h*31U+g->state.screen; h=h*31U+g->state.paused;
    h=h*31U+g->state.lang;
    return h*31U+g->state.tick;
}
