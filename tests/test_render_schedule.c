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
static void save_preview(const char *directory,const char *name)
{
    char path[512];
    assert(snprintf(path,sizeof path,"%s/%s.ppm",directory,name)<(int)sizeof path);
    FILE *file=fopen(path,"wb"); assert(file);
    fprintf(file,"P6\n480 480\n255\n");
    for(size_t i=0;i<GAME_WIDTH*GAME_HEIGHT;++i) {
        const unsigned v=pixels[i];
        fputc((int)((v>>11)*255u/31u),file);
        fputc((int)(((v>>5)&63u)*255u/63u),file);
        fputc((int)((v&31u)*255u/31u),file);
    }
    assert(fclose(file)==0);
}
static uint32_t battery_hash(void)
{
    uint32_t h=2166136261u;
    for(unsigned y=10;y<34;++y) for(unsigned x=16;x<92;++x)
        h=(h^pixels[y*GAME_WIDTH+x])*16777619u;
    return h;
}
static void assert_battery_fill(unsigned percent,uint16_t color)
{
    const unsigned width=15u*percent/100u;
    const uint16_t bg=(uint16_t)((14u>>3)<<11 | (19u>>2)<<5 | (28u>>3));
    for(unsigned y=18;y<28;++y) for(unsigned x=0;x<15;++x)
        assert(pixels[y*GAME_WIDTH+20+x]==(x<width?color:bg));
}
int main(int argc,char **argv)
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
    assert(game_read(game)->tower.floor == 0u);
    assert(game_read(game)->tower.x == 5u && game_read(game)->tower.y == 9u);
    /* Floor one starts at D=(5,9). Take the yellow key at (7,9), return to the
     * stair column, fight the slime at (5,7), open the yellow door at (5,6),
     * then collect the potion at (3,4). */
    press(game, GAME_ACTION_RIGHT);            /* (6,9) */
    press(game, GAME_ACTION_RIGHT);            /* yellow key (7,9) */
    press(game, GAME_ACTION_LEFT);
    press(game, GAME_ACTION_LEFT);             /* back to (5,9) */
    press(game, GAME_ACTION_UP);               /* (5,8) */
    press(game, GAME_ACTION_UP);               /* slime (5,7): combat */
    assert(game_read(game)->tower.phase == TOWER_PHASE_COMBAT);
    press(game, GAME_ACTION_START);            /* confirm the fight */
    press(game, GAME_ACTION_UP);               /* yellow door (5,6) */
    press(game, GAME_ACTION_UP);
    press(game, GAME_ACTION_UP);
    press(game, GAME_ACTION_LEFT);
    press(game, GAME_ACTION_LEFT);             /* potion (3,4) */
    paint_once(game);
    assert(game_read(game)->tower.x == 3u && game_read(game)->tower.y == 4u);
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
    /* After releasing the slider the map still accepts ordinary input. */
    press(game, GAME_ACTION_RIGHT);
    paint_once(game);
    assert(game_read(game)->tower.x == 4u && game_read(game)->tower.y == 4u);
    /* Real production game/view: simulated telemetry is injected through the
     * same public API as the device; no alternate renderer or fake layout. */
    static const unsigned fill_levels[]={0,25,50,67,100};
    for(unsigned i=0;i<sizeof(fill_levels)/sizeof(fill_levels[0]);++i) {
        game_set_battery(game,fill_levels[i],GAME_BATTERY_CHARGING);paint_once(game);
        assert_battery_fill(fill_levels[i],(uint16_t)((127u>>3)<<11 | (216u>>2)<<5 | (180u>>3)));
    }
    game_set_battery(game,67,GAME_BATTERY_DISCHARGING);paint_once(game);
    assert_battery_fill(67,(uint16_t)((234u>>3)<<11 | (230u>>2)<<5 | (215u>>3)));
    assert(pixels[16*GAME_WIDTH+18]==(uint16_t)((234u>>3)<<11 | (230u>>2)<<5 | (215u>>3)));
    const uint32_t normal=battery_hash();
    if(argc>1) save_preview(argv[1],"battery-map-normal");
    game_set_battery(game,67,GAME_BATTERY_CHARGING);paint_once(game);
    assert(pixels[16*GAME_WIDTH+18]==(uint16_t)((127u>>3)<<11 | (216u>>2)<<5 | (180u>>3)));
    const uint32_t charging=battery_hash(); assert(charging!=normal);
    if(argc>1) save_preview(argv[1],"battery-map-charging");
    tap(game,GAME_SETTINGS_X+20,GAME_SETTINGS_Y+15);paint_once(game);
    assert(battery_hash()==charging);
    if(argc>1) save_preview(argv[1],"battery-settings-charging");
    game_set_battery(game,100,GAME_BATTERY_CHARGING);paint_once(game);
    assert(battery_hash()!=charging);
    if(argc>1) save_preview(argv[1],"battery-settings-full");
    game_set_battery(game,12,GAME_BATTERY_DISCHARGING);paint_once(game);
    if(argc>1) save_preview(argv[1],"battery-settings-low");
    idle_acquisitions=acquisitions;idle_presents=presents;
    for(int i=0;i<300;++i) {
        game_set_battery(game,12,GAME_BATTERY_DISCHARGING);game_update(game);
        assert(game_render_device_pending(game) && game_is_idle(game));
    }
    assert(acquisitions==idle_acquisitions && presents==idle_presents);
    game_delete(game);
    assert(game_create(&config,&game));
    game_set_battery(game,67,GAME_BATTERY_CHARGING);paint_once(game);
    if(argc>1) save_preview(argv[1],"battery-title-charging");
    assert(battery_hash()==charging);
    game_delete(game);
    CloseWindow();
    puts("PASS: real RGB565 acquire/submit retry; 300 idle polls = 0 acquisitions/presents; input, static sprites across active ticks, sticky feedback and fresh-target screenshots");
    return 0;
}
