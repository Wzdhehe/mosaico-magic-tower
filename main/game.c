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
    game_preferences_t saved_preferences;
    uint8_t brightness_requested;
    bool brightness_pending;
    bool press_settings;
    enum { POINTER_NONE, POINTER_VOLUME, POINTER_BRIGHTNESS } pointer_control;
    game_screen_t press_screen;
    tower_phase_t press_phase;
};
static void refresh(game_handle_t g) { (void)tower_read(g->model, &g->state.tower); }
static void set_screen(game_handle_t g, game_screen_t screen)
{
    if (g->state.screen != screen) { g->state.screen=screen; g->frame_pending=true; }
}
static void suppress_directions(game_handle_t g)
{
    for (int code=0; code<7; ++code)
        if (code==GAME_ACTION_LEFT || code==GAME_ACTION_RIGHT || code==GAME_ACTION_UP || code==GAME_ACTION_DOWN)
            if (g->held[code]) g->suppressed[code]=true;
    g->repeat=0;
}
static game_preferences_t preferences(game_handle_t g)
{
    return (game_preferences_t){g->state.lang,g->state.volume,g->state.brightness};
}
static void apply_brightness(game_handle_t g)
{
    if (!g->brightness_pending) return;
    g->brightness_pending=false;
    const uint8_t value=g->brightness_requested;
    if (value==g->state.brightness) return;
    if (g->config.set_brightness && !g->config.set_brightness(value)) return;
    g->state.brightness=value;
    g->frame_pending=true;
}
static void set_settings_open(game_handle_t g, bool open)
{
    if (g->state.settings_open==open) return;
    suppress_directions(g);
    if (!open) {
        /* Flush the final coalesced sample before persisting the applied value. */
        apply_brightness(g);
        const game_preferences_t value=preferences(g);
        if (value.lang!=g->saved_preferences.lang || value.volume!=g->saved_preferences.volume ||
            value.brightness!=g->saved_preferences.brightness || g->state.preferences_save_failed) {
            const bool ok=g->config.save_preferences &&
                g->config.save_preferences(g->config.preferences_context,&value);
            g->state.preferences_save_failed=!ok;
            if (ok) g->saved_preferences=value;
        }
    }
    g->state.settings_open=open;
    g->frame_pending=true;
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
    g->state.settings_open = false;
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
    g->state.settings_open = false;
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
static int slider_value(int x, int bx, int width, int minimum, int maximum)
{
    if (x<bx) x=bx;
    if (x>bx+width) x=bx+width;
    return minimum+(x-bx)*(maximum-minimum)/width;
}
static bool in_slider(int x, int y, int bx, int by, int width, int height)
{
    return inside(x,y,bx,by-GAME_SETTINGS_SLIDER_MARGIN,width+1,
                  height+2*GAME_SETTINGS_SLIDER_MARGIN);
}
static void update_slider(game_handle_t g, int x)
{
    if (g->pointer_control==POINTER_VOLUME)
        set_volume(g,slider_value(x,GAME_SETTINGS_VOLUME_X,GAME_SETTINGS_VOLUME_W,0,100));
    else if (g->pointer_control==POINTER_BRIGHTNESS) {
        const uint8_t requested=(uint8_t)slider_value(x,GAME_SETTINGS_BRIGHTNESS_X,
            GAME_SETTINGS_BRIGHTNESS_W,GAME_BRIGHTNESS_MIN,GAME_BRIGHTNESS_MAX);
        if (requested==g->brightness_requested) return;
        g->brightness_requested=requested;
        g->brightness_pending=requested!=g->state.brightness;
    }
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
    g->state.brightness = GAME_BRIGHTNESS_DEFAULT;
    game_preferences_t loaded;
    if (config->load_preferences && config->load_preferences(config->preferences_context,&loaded) &&
        loaded.lang<=GAME_LANG_EN && loaded.volume<=100 &&
        loaded.brightness>=GAME_BRIGHTNESS_MIN && loaded.brightness<=GAME_BRIGHTNESS_MAX) {
        g->state.lang=loaded.lang;
        g->state.volume=loaded.volume;
        g->state.brightness=loaded.brightness;
    }
    g->saved_preferences=preferences(g);
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
    set_settings_open(g,false);
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
    if (g->state.settings_open) {
        if (inside(x,y,GAME_SETTINGS_CLOSE_X,GAME_SETTINGS_CLOSE_Y,GAME_SETTINGS_CLOSE_W,GAME_SETTINGS_CLOSE_H) ||
            !inside(x,y,GAME_SETTINGS_PANEL_X,GAME_SETTINGS_PANEL_Y,GAME_SETTINGS_PANEL_W,GAME_SETTINGS_PANEL_H))
            set_settings_open(g,false);
        else if (inside(x,y,GAME_SETTINGS_ZH_X,GAME_SETTINGS_ZH_Y,GAME_SETTINGS_ZH_W,GAME_SETTINGS_ZH_H)) {
            if (g->state.lang!=GAME_LANG_ZH) { g->state.lang=GAME_LANG_ZH; g->frame_pending=true; }
        } else if (inside(x,y,GAME_SETTINGS_EN_X,GAME_SETTINGS_EN_Y,GAME_SETTINGS_EN_W,GAME_SETTINGS_EN_H)) {
            if (g->state.lang!=GAME_LANG_EN) { g->state.lang=GAME_LANG_EN; g->frame_pending=true; }
        }
        return;
    }
    if (inside(x,y,GAME_SETTINGS_TOUCH_X,GAME_SETTINGS_TOUCH_Y,GAME_SETTINGS_TOUCH_W,GAME_SETTINGS_TOUCH_H)) {
        set_settings_open(g,true);
        return;
    }
    if (g->state.paused) return;
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
        /* Pointer capture suppresses directions without inventing releases
         * for held START/RESET/PAUSE keys. Only their real release rearms them. */
        suppress_directions(g);
        g->press_settings=g->state.settings_open;
        g->pointer_control=POINTER_NONE;
        if (g->press_settings) {
            if (!g->brightness_pending) g->brightness_requested=g->state.brightness;
            if (in_slider(x,y,GAME_SETTINGS_VOLUME_X,GAME_SETTINGS_VOLUME_Y,GAME_SETTINGS_VOLUME_W,GAME_SETTINGS_VOLUME_H))
                g->pointer_control=POINTER_VOLUME;
            else if (in_slider(x,y,GAME_SETTINGS_BRIGHTNESS_X,GAME_SETTINGS_BRIGHTNESS_Y,GAME_SETTINGS_BRIGHTNESS_W,GAME_SETTINGS_BRIGHTNESS_H))
                g->pointer_control=POINTER_BRIGHTNESS;
        }
    }
    g->state.pointer_x=x; g->state.pointer_y=y; g->state.pointer_down=pressed;
    if (pressed && g->press_settings && g->state.settings_open) update_slider(g,x);
    if (pressed || !was_down) return;
    if (g->press_settings) {
        if (g->pointer_control==POINTER_NONE && g->state.settings_open &&
            abs(release_x-g->press_x)<10 && abs(release_y-g->press_y)<10)
            tap(g,release_x,release_y);
        g->pointer_control=POINTER_NONE;
        return;
    }
    if (g->state.settings_open) return;
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
    if (g->state.settings_open) {
        if (code==GAME_ACTION_START || code==GAME_ACTION_LEFT) set_settings_open(g,false);
        return;
    }
    if (code==GAME_ACTION_PAUSE) { game_set_paused(g,!g->state.paused); return; }
    if (g->state.paused) return;
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
    if (g->state.pointer_down) {
        if (pressed) g->suppressed[code]=true;
        g->held[code]=false; return;
    }
    const bool rising=pressed && !g->held[code];
    g->held[code]=pressed;
    if (rising) {
        g->repeat=0;
        const bool settings=g->state.settings_open;
        action(g,code);
        if (settings) suppress_directions(g);
    }
}
static uint8_t expected_music(game_handle_t g)
{ return g->state.screen==GAME_TITLE ? 0 : (g->state.tower.floor>=5 ? 2 : 1); }
static bool repeating_direction(game_handle_t g)
{
    if (g->state.paused || g->state.pointer_down || g->state.settings_open ||
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
{ return !g || (!g->frame_pending && !g->brightness_pending && expected_music(g)==g->state.music && !repeating_direction(g)); }
void game_update(game_handle_t g)
{
    if (!g) return;
    apply_brightness(g);
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
