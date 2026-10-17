// SPDX-License-Identifier: MIT
#include "game_audio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_RATE 24000U
#define AUDIO_CHUNK 240U
#define TRACK_COUNT 3
#define STEPS_PER_TRACK 64U
#define DELAY_SAMPLES 3600U
#define TAG "magic_tower_audio"

/* Three chiptune loops share one 64-step contour: a bright title theme, the
 * same motif a fourth higher for the tower, and a minor-key, faster version
 * for floors six and above. The echo line is the only sizeable buffer. */
static const uint8_t s_melody[TRACK_COUNT][STEPS_PER_TRACK]={
    {60,64,67,64, 57,60,64,60, 53,57,60,57, 55,59,62,67,
     72,67,64,67, 69,64,60,64, 65,60,57,60, 67,62,59,55,
     64,67,72,67, 65,69,72,69, 62,65,69,65, 64,67,71,74,
     72,67,64,60, 62,59,55,59, 60,64,67,72, 67,64,60,55},
    {65,69,72,69, 62,65,69,65, 58,62,65,62, 60,64,67,72,
     77,72,69,72, 74,69,65,69, 70,65,62,65, 72,67,64,60,
     69,72,77,72, 70,74,77,74, 67,70,74,70, 69,72,76,79,
     77,72,69,65, 67,64,60,64, 65,69,72,77, 72,69,65,60},
    {57,60,64,60, 55,59,62,59, 53,57,60,57, 52,56,59,64,
     69,64,60,64, 67,62,59,62, 65,60,57,60, 64,59,56,52,
     60,64,69,64, 62,65,69,65, 60,64,67,64, 59,62,67,71,
     69,64,60,57, 62,59,55,59, 60,64,67,72, 64,60,57,52},
};
static const uint8_t s_bass[TRACK_COUNT][8]={
    {48,45,41,43, 48,45,41,43},
    {53,50,46,48, 53,50,46,48},
    {45,43,41,40, 45,43,41,40},
};
static const uint16_t s_step_samples[TRACK_COUNT]={9000,6000,6000};
static const uint16_t s_lead_gain[TRACK_COUNT]={1600,1750,1500};
static const uint16_t s_pulse_gain[TRACK_COUNT]={0,0,420};

static volatile int s_volume=45;
static volatile int s_track=0;
static volatile bool s_running;
static TaskHandle_t s_task;
static int16_t s_delay[DELAY_SAMPLES];

static uint32_t note_hz(uint8_t midi)
{
    static const uint16_t base[12]={262,277,294,311,330,349,370,392,415,440,466,494};
    int octave=(int)midi/12-5;
    uint32_t hz=base[midi%12];
    if(octave>0) hz<<=octave;
    else if(octave<0) hz>>=-octave;
    return hz;
}

static int32_t triangle(uint32_t phase)
{
    const uint32_t level=(phase>>8)&0xFFU;
    return level<128U?(int32_t)level*2-128:(int32_t)(128U-level)*2;
}

static int32_t pulse(uint32_t phase)
{
    return ((phase>>9)&0xFFU)<64U?96:-96;
}

static void audio_task(void *argument)
{
    (void)argument;
    const i2s_std_config_t i2s_config={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={
            .mclk=BSP_AUDIO_I2S_MCLK,.bclk=BSP_AUDIO_I2S_SCLK,.ws=BSP_AUDIO_I2S_LRCLK,
            .dout=BSP_AUDIO_I2S_SDOUT,.din=GPIO_NUM_NC
        }
    };
    i2s_std_config_t config=i2s_config;
    config.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
    esp_codec_dev_handle_t codec=NULL;
    if(bsp_audio_init(&config)==ESP_OK) codec=bsp_audio_codec_speaker_init();
    esp_codec_dev_sample_info_t info={
        .sample_rate=AUDIO_RATE,.bits_per_sample=16,.channel=1,.channel_mask=0
    };
    if(!codec||esp_codec_dev_open(codec,&info)!=ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG,"speaker unavailable; continuing without BGM");
        s_running=false;
        s_task=NULL;
        vTaskDelete(NULL);
        return;
    }
    esp_codec_dev_set_out_vol(codec,80);
    ESP_LOGI(TAG,"chiptune BGM started at %u Hz",AUDIO_RATE);
    int16_t out[AUDIO_CHUNK];
    uint32_t sample=0,phase=0,bass_phase=0,pulse_phase=0;
    uint32_t delay_index=0;
    int active=s_track;
    uint32_t write_errors=0;
    memset(s_delay,0,sizeof(s_delay));
    while(s_running) {
        const int track=s_track>=0&&s_track<TRACK_COUNT?s_track:0;
        if(track!=active) {
            active=track;
            sample=0;
            phase=bass_phase=pulse_phase=0;
            delay_index=0;
            memset(s_delay,0,sizeof(s_delay));
            ESP_LOGI(TAG,"BGM track %d",active);
        }
        const int volume=s_volume;
        const uint32_t step_samples=s_step_samples[active];
        const uint32_t steps=sizeof(s_melody[0]);
        for(unsigned i=0;i<AUDIO_CHUNK;++i,++sample) {
            const uint32_t step=(sample/step_samples)%steps;
            const uint32_t position=sample%step_samples;
            const uint32_t note_index=(sample/(step_samples*8U))%8U;
            const uint8_t note=s_melody[active][step];
            const uint32_t melody_hz=note_hz(note);
            if(position==0) {
                phase=0;
                pulse_phase=0;
            }
            phase+=(uint32_t)((uint64_t)melody_hz*65536U/AUDIO_RATE);
            if(position==0) bass_phase=0;
            bass_phase+=(uint32_t)((uint64_t)note_hz(s_bass[active][note_index])*65536U/AUDIO_RATE);
            if(position==0) pulse_phase=0;
            pulse_phase+=(uint32_t)((uint64_t)(melody_hz*2U)*65536U/AUDIO_RATE);
            int32_t envelope=256;
            if(position<180U) envelope=(int32_t)(position*256U/180U);
            else if(position+900U>step_samples)
                envelope=(int32_t)((step_samples-position)*256U/900U);
            if(envelope<0) envelope=0;
            const int32_t lead=triangle(phase)*envelope/256;
            const int32_t bass=triangle(bass_phase);
            const int32_t arp=pulse(pulse_phase)*s_pulse_gain[active]/256;
            const int32_t echo=s_delay[delay_index]*5/16;
            s_delay[delay_index]=(int16_t)(lead*s_lead_gain[active]/256);
            if(++delay_index>=DELAY_SAMPLES) delay_index=0;
            int32_t mixed=lead*(s_lead_gain[active]*volume/100)/256+
                          bass*(720*volume/100)/256+
                          arp*(volume)/100+
                          echo*(volume)/100;
            if(mixed>32767) mixed=32767;
            if(mixed<-32768) mixed=-32768;
            out[i]=(int16_t)mixed;
        }
        if(esp_codec_dev_write(codec,out,sizeof(out))!=ESP_CODEC_DEV_OK) {
            if((write_errors++%100U)==0U) ESP_LOGW(TAG,"speaker write failed");
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    esp_codec_dev_close(codec);
    s_task=NULL;
    vTaskDelete(NULL);
}

void game_audio_init(void)
{
    if(s_task) return;
    s_running=true;
    if(xTaskCreate(audio_task,"magic_tower_audio",4096,NULL,6,&s_task)!=pdPASS) {
        s_running=false;
        s_task=NULL;
        ESP_LOGW(TAG,"could not start BGM task");
    }
}

void game_audio_set_volume(int volume)
{
    if(volume<0) volume=0;
    if(volume>100) volume=100;
    s_volume=volume;
}

void game_audio_set_track(int track)
{
    if(track<0) track=0;
    if(track>=TRACK_COUNT) track=TRACK_COUNT-1;
    s_track=track;
}
