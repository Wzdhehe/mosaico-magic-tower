// SPDX-License-Identifier: MIT
/* Production audio loop, real score/DSP, deterministic fake RTOS and codec. */
#include "audio_test_platform.h"
#include "game_audio.h"
#include "game_music.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TaskFunction_t entry;
static void *argument;
static jmp_buf done;
static const char *scenario;
static int64_t now;
static unsigned initialized,opened,closed,writes,waits,notifications,muted_writes;
static unsigned sounding,cycles,stage,active_track,advances,task_creations;
static unsigned factories;
static bool hardware_open,hardware_muted=true,failed,requested_zero;
static int token;
static int16_t last_played;
static bool checked_fade;
static bool is(const char *s) {return strcmp(scenario,s)==0;}
static bool fail_once(const char *s) {if(is(s)&&!failed){failed=true;return true;}return false;}
int64_t esp_timer_get_time(void) {return now;}
int xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *h)
{ (void)name;assert(stack==4096 && priority==6);++task_creations;entry=fn;argument=arg;if(h)*h=&token;return pdPASS; }
TaskHandle_t xTaskGetCurrentTaskHandle(void){return &token;}
void xTaskNotifyGive(TaskHandle_t h){assert(h==&token);++notifications;}
void vTaskDelay(TickType_t ticks){now+=(int64_t)ticks*1000;}
void vTaskDelete(TaskHandle_t h){(void)h;assert(!"audio task must retain its published handle");}
esp_err_t bsp_audio_init(const i2s_std_config_t *c)
{ assert(c->clk_cfg.sample_rate_hz==24000 && c->clk_cfg.mclk_multiple==256);++initialized;return fail_once("init-fail")?-1:0; }
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void){++factories;return is("speaker-fail")?NULL:&token;}
int esp_codec_dev_open(esp_codec_dev_handle_t h,esp_codec_dev_sample_info_t *info)
{ assert(h==&token && info->channel==1 && info->bits_per_sample==16);++opened;hardware_open=true;hardware_muted=false;return fail_once("open-fail")?-1:0; }
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t h,int volume)
{assert(h==&token && volume==100);return fail_once("volume-fail")?-1:0;}
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t h,bool mute)
{assert(h==&token && hardware_open);if(fail_once("mute-fail") || (!mute && fail_once("unmute-fail")))return -1;hardware_muted=mute;return 0;}
int esp_codec_dev_close(esp_codec_dev_handle_t h)
{assert(h==&token);++closed;if(fail_once("close-fail"))return -1;hardware_open=false;hardware_muted=true;++cycles;return 0;}
bool audio_test_track(game_music_handle_t music,unsigned track)
{active_track=track;return game_music_set_track(music,track);}
bool audio_test_advance(game_music_handle_t music,uint64_t frames)
{++advances;return game_music_advance_muted(music,frames);}
int esp_codec_dev_write(esp_codec_dev_handle_t h,void *data,int bytes)
{
    assert(h==&token && hardware_open && bytes==480);assert(++writes<250);
    const int16_t *pcm=data;bool nonzero=false;
    for(unsigned i=0;i<240;++i)if(pcm[i])nonzero=true;
    if(hardware_muted) {assert(!nonzero);++muted_writes;}
    if(is("warmup-mute") && hardware_muted && stage==0 && !requested_zero) {
        requested_zero=true;game_audio_set_volume(0);
    }
    if(!hardware_muted && fail_once("write-fail"))return -1;
    now+=10000;
    if(!hardware_muted && nonzero && !requested_zero) {
        last_played=pcm[239];
        ++sounding;
        if(stage>=3) {
            if(is("cycle-repeat") && cycles<6) {
                stage=0;checked_fade=false;requested_zero=true;game_audio_set_volume(0);return 0;
            }
            assert(active_track==9 && sounding>=(is("warmup-mute")?1u:3u));
            assert(opened>=2 && closed>=1 && advances>0 && muted_writes>=12);
            assert(task_creations==1);
            longjmp(done,1);
        }
        if(sounding>=3 && (is("cycle-positive")?last_played>100:last_played < -100)) {
            requested_zero=true;game_audio_set_volume(0);
        }
    } else if(!hardware_muted && nonzero && requested_zero && !checked_fade) {
        assert(pcm[0]==last_played && pcm[239]==0);
        for(unsigned i=1;i<240;++i) {
            assert(abs(pcm[i])<=abs(pcm[i-1]));
            assert(pcm[i]==0 || (pcm[i]>0)==(last_played>0));
        }
        checked_fade=true;
    }
    return 0;
}
uint32_t ulTaskNotifyTake(int clear,TickType_t ticks)
{
    assert(clear==pdTRUE);assert(++waits<30);
    if(is("init-fail") || is("speaker-fail")) {
        /* Real BSP factories can retain partially allocated resources. They
         * must never be called repeatedly after initial construction fails. */
        assert(ticks==portMAX_DELAY && initialized==1 && writes==0 && opened==0);
        assert(factories==(is("speaker-fail")?1u:0u));
        if(waits>=4)longjmp(done,1);
        now+=1000000;game_audio_set_volume((int)(40u+waits));return 1;
    }
    if(ticks!=portMAX_DELAY) {
        assert(ticks>0 && ticks<=1000);now+=(int64_t)ticks*1000;return 0;
    }
    assert(!hardware_open); /* Truly parked, not an endless zero-PCM loop. */
    if(is("boot-muted") && stage==0 && !requested_zero) {
        assert(initialized==0 && opened==0 && writes==0);
        now+=1000000;game_audio_set_volume(45);return 1;
    }
    assert(requested_zero && closed>0);
    if(is("warmup-mute") && stage==0)assert(sounding==0);
    const unsigned before=writes,notify_before=notifications;
    for(unsigned i=0;i<300;++i)game_audio_set_volume(0);
    assert(notifications==notify_before && writes==before);
    if(stage==0) {++stage;now+=INT64_C(3600000000);return 0;}
    if(stage==1) {++stage;now+=1000000;game_audio_set_track(9);return 1;}
    assert(stage==2);++stage;now+=5000000;requested_zero=false;game_audio_set_volume(70);return 1;
}
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];
    game_audio_set_volume(is("boot-muted")?0:45);
    game_audio_init();game_audio_init();assert(task_creations==1);
    if(!setjmp(done))entry(argument);
    assert(checked_fade || is("warmup-mute") || is("init-fail") || is("speaker-fail"));
    assert(failed || is("speaker-fail") || is("warmup-mute") || is("cycle") || is("cycle-positive") || is("cycle-repeat") || is("boot-muted"));
    printf("PASS: %s: parked with codec closed; wake/current track/fades/error retry; writes=%u waits=%u\n",scenario,writes,waits);
    return 0;
}
