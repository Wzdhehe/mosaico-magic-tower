// SPDX-License-Identifier: MIT
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "esp_check.h"
#include "game.h"
#include "game_battery.h"
#include "esp_timer.h"
#include "game_audio.h"
#include "game_config.h"
#include "game_device_render.h"
#include "game_preferences.h"
#include "mosaico_game_app.h"
#include "raylib_screen_mirror.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Owned by app_main for the application lifetime. Engine callbacks execute on
 * that same task; touch samples reach it through the engine event queue. */
static game_handle_t s_game;

static bool apply_brightness(int percent)
{
    const esp_err_t result=mosaico_game_app_set_brightness(percent);
    if (result!=ESP_OK) ESP_LOGW(GAME_NAME,"Brightness unavailable: %s",esp_err_to_name(result));
    return result==ESP_OK;
}
static bool load_preferences(void *context,game_preferences_t *out)
{
    (void)context;
    nvs_handle_t nvs;
    if (nvs_open("magic_tower",NVS_READONLY,&nvs)!=ESP_OK) return false;
    uint32_t packed=0;
    const esp_err_t result=nvs_get_u32(nvs,"prefs1",&packed);
    nvs_close(nvs);
    if (result!=ESP_OK) return false;
    uint8_t bytes[GAME_PREFERENCES_BYTES];
    for(unsigned i=0;i<GAME_PREFERENCES_BYTES;++i) bytes[i]=(uint8_t)(packed>>(8u*i));
    return game_preferences_decode(bytes,out);
}
static bool save_preferences(void *context,const game_preferences_t *value)
{
    (void)context;
    uint8_t bytes[GAME_PREFERENCES_BYTES];
    if (!game_preferences_encode(value,bytes)) return false;
    uint32_t packed=0;
    for(unsigned i=0;i<GAME_PREFERENCES_BYTES;++i) packed|=(uint32_t)bytes[i]<<(8u*i);
    nvs_handle_t nvs;
    if (nvs_open("magic_tower",NVS_READWRITE,&nvs)!=ESP_OK) return false;
    esp_err_t result=nvs_set_u32(nvs,"prefs1",packed);
    if (result==ESP_OK) result=nvs_commit(nvs);
    nvs_close(nvs);
    if (result!=ESP_OK) ESP_LOGW(GAME_NAME,"Settings save failed: %s",esp_err_to_name(result));
    return result==ESP_OK;
}

static bool save_nvs(void *context, const uint8_t *bytes, size_t size)
{
    (void)context;
    nvs_handle_t nvs;
    esp_err_t result = nvs_open("magic_tower", NVS_READWRITE, &nvs);
    if (result != ESP_OK) return false;
    result = nvs_set_blob(nvs, "journey", bytes, size);
    if (result == ESP_OK) result = nvs_commit(nvs);
    nvs_close(nvs);
    if (result != ESP_OK) ESP_LOGW(GAME_NAME, "Save failed: %s", esp_err_to_name(result));
    return result == ESP_OK;
}

static bool load_nvs(void *context, uint8_t *bytes, size_t size)
{
    (void)context;
    nvs_handle_t nvs;
    if (nvs_open("magic_tower", NVS_READONLY, &nvs) != ESP_OK) return false;
    size_t stored = 0;
    esp_err_t result = nvs_get_blob(nvs, "journey", NULL, &stored);
    if (result == ESP_OK && stored == size) result = nvs_get_blob(nvs, "journey", bytes, &stored);
    else result = ESP_ERR_INVALID_SIZE;
    nvs_close(nvs);
    return result == ESP_OK && stored == size;
}

static void on_event(const mosaico_device_event_t *event)
{
    if (event && event->type == MOSAICO_DEVICE_EVENT_POINTER)
        game_set_pointer(s_game, event->x, event->y, event->pressed);
}

static void on_update(void)
{
    game_update(s_game);
}

static void on_render(void)
{
    (void)game_render_device_pending(s_game);
}

static esp_err_t after_healthy(void)
{
    /* The first physical frame and startup flush have completed. A retained
     * resume token can now be retried by the normal engine loop. */
    const game_snapshot_t *state=game_read(s_game);
    if (state) (void)apply_brightness(state->brightness);
    return ESP_OK;
}

/* App-lifetime telemetry; the same UI task owns creation, polling and release. */
static game_battery_handle_t s_battery;
static void poll_battery(void)
{
    game_battery_reading_t reading;
    if(game_battery_poll(s_battery,esp_timer_get_time(),&reading))
        game_set_battery(s_game,reading.percent,reading.state);
}
static bool on_idle(void)
{
    poll_battery();
    return game_is_idle(s_game);
}

static esp_err_t on_start(void)
{
    const esp_err_t battery=game_battery_create(&s_battery);
    if(battery!=ESP_OK) ESP_LOGW(GAME_NAME,"Battery unavailable: %s",esp_err_to_name(battery));
    poll_battery();
    const game_snapshot_t *state=game_read(s_game);
    game_audio_set_volume(state?state->volume:45);
    game_audio_set_track(state?state->music:0);
    /* Publish saved controls before the higher-priority audio task can run. */
    game_audio_init();
    return ESP_OK;
}

static const mosaico_game_app_config_t s_config = {
    .tag = GAME_NAME,
    .window_title = GAME_NAME,
    .canvas_bind = GSP_GAME_BIND_GAME_CANVAS,
    .touch_points = 1,
    .target_fps = GAME_TICK_HZ,
    .drawbuf_lines = 34,
    .te_compose_buffers = 2,
    .te_sync = true,
    .gsp_bundle = gsp_bundle_config,
    .register_mirror = raylib_screen_mirror_register,
    .on_start = on_start,
    .after_healthy = after_healthy,
    .on_event = on_event,
    .idle = on_idle,
    .on_update = on_update,
    .on_render = on_render,
};

void app_main(void)
{
    /* Never erase NVS to recover an init failure: other applications may own data. */
    esp_err_t storage = nvs_flash_init();
    if (storage != ESP_OK) ESP_LOGW(GAME_NAME, "Storage unavailable: %s", esp_err_to_name(storage));
    const game_config_t config = {.width = GAME_WIDTH, .height = GAME_HEIGHT,
        .save = storage == ESP_OK ? save_nvs : NULL,
        .load = storage == ESP_OK ? load_nvs : NULL,
        .load_preferences = storage == ESP_OK ? load_preferences : NULL,
        .save_preferences = storage == ESP_OK ? save_preferences : NULL,
        .set_brightness = apply_brightness,
        .set_volume = game_audio_set_volume,
        .set_music = game_audio_set_track};
    ESP_ERROR_CHECK(game_create(&config, &s_game) ? ESP_OK : ESP_ERR_NO_MEM);
    /* The engine starts iris_ota_support_start() and marks the image healthy
     * after presenting its first frame; the OTA writer remains in Recovery. */
    esp_err_t result = mosaico_game_app_run(&s_config);
    game_battery_delete(s_battery);
    s_battery=NULL;
    game_delete(s_game);
    s_game = NULL;
    ESP_ERROR_CHECK(result);
}
