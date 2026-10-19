"""Run the production audio task with fault-injecting SDK/RTOS ports and real DSP."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
PLATFORM=r'''#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ESP_OK 0
#define ESP_CODEC_DEV_OK 0
#define ESP_CODEC_DEV_FAIL -1
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define GPIO_NUM_NC -1
#define BSP_AUDIO_I2S_MCLK 1
#define BSP_AUDIO_I2S_SCLK 2
#define BSP_AUDIO_I2S_LRCLK 3
#define BSP_AUDIO_I2S_SDOUT 4
#define I2S_SLOT_MODE_STEREO 2
#define I2S_MCLK_MULTIPLE_256 256
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) {.sample_rate_hz=(rate)}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_value,mode_value) {.bits=(bit_value),.mode=(mode_value)}
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
typedef int esp_err_t;
typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void*);
typedef struct {struct {unsigned sample_rate_hz,mclk_multiple;} clk_cfg;
 struct {int bits,mode;} slot_cfg;
 struct {int mclk,bclk,ws,dout,din;} gpio_cfg;} i2s_std_config_t;
typedef void *esp_codec_dev_handle_t;
typedef struct {unsigned sample_rate,bits_per_sample,channel,channel_mask;} esp_codec_dev_sample_info_t;
esp_err_t bsp_audio_init(const i2s_std_config_t*);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
int esp_codec_dev_open(esp_codec_dev_handle_t,esp_codec_dev_sample_info_t*);
int esp_codec_dev_close(esp_codec_dev_handle_t);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t,int);
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t,bool);
int esp_codec_dev_write(esp_codec_dev_handle_t,void*,int);
int xTaskCreate(TaskFunction_t,const char*,unsigned,void*,unsigned,TaskHandle_t*);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void xTaskNotifyGive(TaskHandle_t);
uint32_t ulTaskNotifyTake(int,TickType_t);
void vTaskDelay(TickType_t);
void vTaskDelete(TaskHandle_t);
int64_t esp_timer_get_time(void);
'''

def main():
    cc=os.environ.get('CC','gcc')
    with tempfile.TemporaryDirectory(prefix='audio-power-') as td:
        p=Path(td)
        (p/'audio_test_platform.h').write_text(PLATFORM)
        for name in ['bsp/esp_mosaico.h','driver/i2s_std.h','esp_codec_dev.h','esp_log.h',
                     'esp_timer.h','freertos/FreeRTOS.h','freertos/task.h']:
            dest=p/name;dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text('#include "audio_test_platform.h"\n')
        common=[cc,'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2','-I',str(p),'-I',str(ROOT/'main')]
        audio=p/'audio.o'
        subprocess.run([*common,'-Dgame_music_set_track=audio_test_track',
                        '-Dgame_music_advance_muted=audio_test_advance',
                        '-c',str(ROOT/'main/game_audio.c'),'-o',str(audio)],check=True)
        binary=p/('audio.exe' if os.name=='nt' else 'audio')
        subprocess.run([*common,str(ROOT/'tests/test_audio_power.c'),str(ROOT/'main/game_music.c'),
                        str(audio),'-o',str(binary)],check=True)
        for scenario in ['cycle','cycle-positive','cycle-repeat','boot-muted','warmup-mute','init-fail','speaker-fail','open-fail',
                         'volume-fail','mute-fail','unmute-fail','close-fail','write-fail']:
            subprocess.run([str(binary),scenario],check=True,timeout=10)

if __name__=='__main__':main()
