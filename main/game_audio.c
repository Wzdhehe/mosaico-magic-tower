// SPDX-License-Identifier: MIT
#include "game_audio.h"
#include "game_music.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define AUDIO_CHUNK 240U
#define AUDIO_SILENCE_CHUNKS 8U /* Pinned BSP: six 240-frame DMA descriptors. */
#define AUDIO_RETRY_US INT64_C(1000000)
#define TAG "magic_tower_audio"

/* The public API intentionally remains a single device service for the whole
 * application's lifetime. Only its task owns the allocated service/music state.
 * UI setters publish controls and notify the task; no ISR callers expected.
 * Once created, the task and its published handle live until application reset,
 * including hardware failures. Never delete/free a task another core may notify. */
static atomic_int s_volume=ATOMIC_VAR_INIT(45);
static atomic_int s_track=ATOMIC_VAR_INIT(0);
static atomic_bool s_started=ATOMIC_VAR_INIT(false);
static _Atomic(TaskHandle_t) s_task=ATOMIC_VAR_INIT(NULL);
typedef struct {
    game_music_handle_t music;
    i2s_std_config_t config;
    esp_codec_dev_sample_info_t info;
    int16_t output[AUDIO_CHUNK];
    esp_codec_dev_handle_t codec; /* Borrowed BSP singleton; never delete it. */
    int64_t mute_clock_us, retry_at_us;
    uint32_t fractional_frames;
    int16_t last_sample;
    bool open, codec_muted, playing, close_pending, construction_failed;
} audio_service_t;
static const i2s_std_config_t s_i2s_template={
    .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(GAME_MUSIC_RATE),
    .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16,I2S_SLOT_MODE_STEREO),
    .gpio_cfg={
        .mclk=BSP_AUDIO_I2S_MCLK,.bclk=BSP_AUDIO_I2S_SCLK,.ws=BSP_AUDIO_I2S_LRCLK,
        .dout=BSP_AUDIO_I2S_SDOUT,.din=GPIO_NUM_NC
    }
};
static void catch_up_muted(audio_service_t *s)
{
    const int64_t now=esp_timer_get_time();
    const uint64_t elapsed=now>s->mute_clock_us?(uint64_t)(now-s->mute_clock_us):0;
    /* 24 kHz = 24 frames / 1000 us; retain the fractional frame across wakes. */
    const uint64_t scaled=elapsed*24u+s->fractional_frames;
    (void)game_music_advance_muted(s->music,scaled/1000u);
    s->fractional_frames=(uint32_t)(scaled%1000u);
    s->mute_clock_us=now;
}
static void enter_muted(audio_service_t *s)
{
    if(!s->playing) return;
    (void)game_music_set_volume(s->music,0);
    s->playing=false;
    s->mute_clock_us=esp_timer_get_time();
    s->fractional_frames=0;
}
static bool close_output(audio_service_t *s)
{
    if(!s->open) return true;
    if(!s->codec_muted && !s->close_pending) {
        /* Ramp the last played sample to zero, then replace the queued DMA
         * buffers with silence before shutting down or reusing them. */
        for(unsigned i=0;i<AUDIO_CHUNK;++i)
            s->output[i]=(int16_t)((int32_t)s->last_sample*(int32_t)(AUDIO_CHUNK-1u-i)/(int32_t)(AUDIO_CHUNK-1u));
        bool written=esp_codec_dev_write(s->codec,s->output,sizeof(s->output))==ESP_CODEC_DEV_OK;
        memset(s->output,0,sizeof(s->output));
        for(unsigned i=0;written && i<AUDIO_SILENCE_CHUNKS;++i)
            written=esp_codec_dev_write(s->codec,s->output,sizeof(s->output))==ESP_CODEC_DEV_OK;
    }
    s->close_pending=true;
    if(esp_codec_dev_set_out_mute(s->codec,true)!=ESP_CODEC_DEV_OK)
        ESP_LOGW(TAG,"speaker mute failed; closing output");
    if(esp_codec_dev_close(s->codec)!=ESP_CODEC_DEV_OK) return false;
    s->open=false;s->codec_muted=true;s->close_pending=false;s->last_sample=0;
    return true;
}
static bool open_output(audio_service_t *s)
{
    if(!s->codec) {
        /* The pinned BSP does not roll back every partial I2S/codec allocation.
         * Attempt its factories once; never leak handles by retrying them. */
        s->construction_failed=true;
        if(bsp_audio_init(&s->config)!=ESP_OK) return false;
        s->codec=bsp_audio_codec_speaker_init();
        if(!s->codec) return false;
        s->construction_failed=false;
    }
    /* open() can fail after partially enabling hardware. Always close that
     * partial state before retrying, retaining the BSP-owned handle. */
    s->open=true;s->close_pending=true;
    if(esp_codec_dev_open(s->codec,&s->info)!=ESP_CODEC_DEV_OK ||
       esp_codec_dev_set_out_mute(s->codec,true)!=ESP_CODEC_DEV_OK ||
       esp_codec_dev_set_out_vol(s->codec,100)!=ESP_CODEC_DEV_OK) {
        (void)close_output(s);return false;
    }
    s->codec_muted=true;
    memset(s->output,0,sizeof(s->output));
    for(unsigned i=0;i<AUDIO_SILENCE_CHUNKS;++i)
        if(esp_codec_dev_write(s->codec,s->output,sizeof(s->output))!=ESP_CODEC_DEV_OK) {
            (void)close_output(s);return false;
        }
    s->close_pending=false;
    return true;
}
static bool wait_retry(audio_service_t *s)
{
    const int64_t remaining=s->retry_at_us-esp_timer_get_time();
    if(remaining<=0) return false;
    const TickType_t ticks=pdMS_TO_TICKS((uint32_t)((remaining+999)/1000));
    (void)ulTaskNotifyTake(pdTRUE,ticks?ticks:1);
    return true;
}
static void failed_output(audio_service_t *s)
{
    enter_muted(s);
    (void)close_output(s);
    s->retry_at_us=esp_timer_get_time()+AUDIO_RETRY_US;
    if(s->construction_failed) ESP_LOGW(TAG,"speaker initialization failed; audio disabled until restart");
    else ESP_LOGW(TAG,"speaker unavailable; retry after one second");
}
static void audio_task(void *argument)
{
    audio_service_t *service=argument;
    /* Config and the 480-byte PCM block belong to the service, keeping large
     * aggregates off the 4096-byte task stack, including failure paths. */
    service->config=s_i2s_template;
    service->config.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
    service->info.sample_rate=GAME_MUSIC_RATE;
    service->info.bits_per_sample=16;
    service->info.channel=1;
    service->info.channel_mask=0;
    (void)game_music_set_volume(service->music,0);
    service->codec_muted=true;
    service->mute_clock_us=esp_timer_get_time();
    atomic_store_explicit(&s_task,xTaskGetCurrentTaskHandle(),memory_order_release);
    for(;;) {
        if(service->construction_failed) {
            /* Keep the notification target alive, but do not retry a factory
             * that may have left BSP-owned allocations behind. */
            (void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            continue;
        }
        if(service->close_pending) {
            if(wait_retry(service)) continue;
            if(!close_output(service)) service->retry_at_us=esp_timer_get_time()+AUDIO_RETRY_US;
            continue;
        }
        const unsigned volume=(unsigned)atomic_load_explicit(&s_volume,memory_order_relaxed);
        if(!volume) {
            enter_muted(service);
            if(!close_output(service)) {
                service->retry_at_us=esp_timer_get_time()+AUDIO_RETRY_US;continue;
            }
            catch_up_muted(service);
            (void)game_music_set_track(service->music,(unsigned)atomic_load_explicit(&s_track,memory_order_relaxed));
            /* No PCM writes, periodic task polling or synthesis while parked.
             * A notification before this wait is retained, avoiding lost wakes. */
            (void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            continue;
        }
        if(!service->open) {
            if(wait_retry(service)) continue;
            catch_up_muted(service);
            (void)game_music_set_track(service->music,(unsigned)atomic_load_explicit(&s_track,memory_order_relaxed));
            if(!open_output(service)) {failed_output(service);continue;}
            catch_up_muted(service);
        }
        /* Controls can change while the codec warms up. Re-read before making
         * any sound; track changes during mute resume the current floor's BGM. */
        const unsigned latest=(unsigned)atomic_load_explicit(&s_volume,memory_order_relaxed);
        if(!latest) continue;
        (void)game_music_set_track(service->music,(unsigned)atomic_load_explicit(&s_track,memory_order_relaxed));
        (void)game_music_set_volume(service->music,latest);
        service->playing=true;
        if(service->codec_muted) {
            if(esp_codec_dev_set_out_mute(service->codec,false)!=ESP_CODEC_DEV_OK) {
                failed_output(service);continue;
            }
            service->codec_muted=false;
        }
        (void)game_music_render(service->music,service->output,AUDIO_CHUNK);
        if(esp_codec_dev_write(service->codec,service->output,sizeof(service->output))!=ESP_CODEC_DEV_OK) {
            failed_output(service);continue;
        }
        service->last_sample=service->output[AUDIO_CHUNK-1u];
    }
}
void game_audio_init(void) {
    bool expected=false;
    if(!atomic_compare_exchange_strong_explicit(&s_started,&expected,true,
                                                memory_order_acq_rel,memory_order_relaxed)) return;
    audio_service_t *service=calloc(1,sizeof(*service));
    if(!service || !game_music_create(NULL,&service->music)) {
        free(service);
        atomic_store_explicit(&s_started,false,memory_order_release);
        ESP_LOGW(TAG,"could not allocate BGM service");
        return;
    }
    if(xTaskCreate(audio_task,"magic_tower_audio",4096,service,6,NULL)!=pdPASS) {
        game_music_delete(service->music);
        free(service);
        atomic_store_explicit(&s_started,false,memory_order_release);
        ESP_LOGW(TAG,"could not start BGM task");
    }
}
void game_audio_set_volume(int volume) {
    if(volume<0) volume=0;
    if(volume>100) volume=100;
    if(atomic_exchange_explicit(&s_volume,volume,memory_order_relaxed)==volume) return;
    TaskHandle_t task=atomic_load_explicit(&s_task,memory_order_acquire);
    if(task) xTaskNotifyGive(task);
}
void game_audio_set_track(int track) {
    if(track<0) track=0;
    if(track>=(int)GAME_MUSIC_TRACKS) track=GAME_MUSIC_TRACKS-1;
    if(atomic_exchange_explicit(&s_track,track,memory_order_relaxed)==track) return;
    TaskHandle_t task=atomic_load_explicit(&s_task,memory_order_acquire);
    if(task) xTaskNotifyGive(task);
}
