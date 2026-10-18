// SPDX-License-Identifier: MIT
#include "game_audio.h"
#include "game_music.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "bsp/esp_mosaico.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define AUDIO_CHUNK 240U
#define TAG "magic_tower_audio"

/* The public API intentionally remains a single device service for the whole
 * application's lifetime. Only its task owns the allocated service/music state.
 * UI setters publish independent controls atomically; no ISR callers expected.
 * There is no shutdown API: successful startup runs until application reset. */
static atomic_int s_volume=ATOMIC_VAR_INIT(45);
static atomic_int s_track=ATOMIC_VAR_INIT(0);
static atomic_bool s_started=ATOMIC_VAR_INIT(false);
typedef struct {
    game_music_handle_t music;
    i2s_std_config_t config;
    esp_codec_dev_sample_info_t info;
    int16_t output[AUDIO_CHUNK];
} audio_service_t;
static const i2s_std_config_t s_i2s_template={
    .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(GAME_MUSIC_RATE),
    .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16,I2S_SLOT_MODE_STEREO),
    .gpio_cfg={
        .mclk=BSP_AUDIO_I2S_MCLK,.bclk=BSP_AUDIO_I2S_SCLK,.ws=BSP_AUDIO_I2S_LRCLK,
        .dout=BSP_AUDIO_I2S_SDOUT,.din=GPIO_NUM_NC
    }
};
static void audio_task(void *argument) {
    audio_service_t *service=argument;
    /* Config and the 480-byte PCM block belong to the service, keeping large
     * aggregates off the 4096-byte task stack, including failure paths. */
    service->config=s_i2s_template;
    service->config.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
    service->info.sample_rate=GAME_MUSIC_RATE;
    service->info.bits_per_sample=16;
    service->info.channel=1;
    service->info.channel_mask=0;
    esp_codec_dev_handle_t codec=NULL;
    if(bsp_audio_init(&service->config)==ESP_OK) codec=bsp_audio_codec_speaker_init();
    if(!codec || esp_codec_dev_open(codec,&service->info)!=ESP_CODEC_DEV_OK) {
        /* BSP owns/caches the codec and I2S interface, so never delete its
         * handle. Close an unsuccessful open before a later startup retry. */
        if(codec) esp_codec_dev_close(codec);
        game_music_delete(service->music);
        free(service);
        ESP_LOGW(TAG,"speaker unavailable; continuing without BGM");
        atomic_store_explicit(&s_started,false,memory_order_release);
        vTaskDelete(NULL);
        return;
    }
    esp_codec_dev_set_out_vol(codec,100);
    ESP_LOGI(TAG,"original BGM started at %u Hz",GAME_MUSIC_RATE);
    uint32_t write_errors=0;
    for(;;) {
        game_music_set_volume(service->music,(unsigned)atomic_load_explicit(&s_volume,memory_order_relaxed));
        game_music_set_track(service->music,(unsigned)atomic_load_explicit(&s_track,memory_order_relaxed));
        game_music_render(service->music,service->output,AUDIO_CHUNK);
        if(esp_codec_dev_write(codec,service->output,sizeof(service->output))!=ESP_CODEC_DEV_OK) {
            if((write_errors++%100U)==0U) ESP_LOGW(TAG,"speaker write failed");
            vTaskDelay(pdMS_TO_TICKS(5));
        }
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
    atomic_store_explicit(&s_volume,volume,memory_order_relaxed);
}
void game_audio_set_track(int track) {
    if(track<0) track=0;
    if(track>=(int)GAME_MUSIC_TRACKS) track=GAME_MUSIC_TRACKS-1;
    atomic_store_explicit(&s_track,track,memory_order_relaxed);
}
