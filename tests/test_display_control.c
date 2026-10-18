/* Test the actual production transaction, extracted by run_display_control.py. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
typedef int esp_err_t;
typedef void *esp_gsp_handle_t;
typedef void *TaskHandle_t;
typedef struct { int id; } esp_gsp_esp_lcd_pause_t;
enum { ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_ARG=2, ESP_ERR_INVALID_STATE=3, ESP_ERR_TIMEOUT=4 };
static esp_gsp_handle_t s_gsp;
static TaskHandle_t s_app_task;
static esp_gsp_esp_lcd_pause_t *s_brightness_pause;
static TaskHandle_t current_task;
static esp_gsp_esp_lcd_pause_t token={1};
static int pauses, writes, resumes, pause_result, write_result, resume_result;
static int step, write_step, resume_step, written;
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static esp_err_t esp_gsp_esp_lcd_pause(esp_gsp_handle_t gsp, uint32_t timeout,
                                      esp_gsp_esp_lcd_pause_t **out)
{
    assert(gsp==s_gsp && timeout==250 && !s_brightness_pause);
    ++pauses; ++step;
    if (pause_result==ESP_OK) *out=&token;
    return pause_result;
}
static esp_err_t bsp_display_brightness_set(int value)
{
    assert(s_brightness_pause==&token);
    ++writes; write_step=++step; written=value;
    return write_result;
}
static esp_err_t esp_gsp_esp_lcd_resume_paused(esp_gsp_esp_lcd_pause_t *pause,
                                             esp_gsp_handle_t *out)
{
    assert(pause==&token);
    ++resumes; resume_step=++step;
    /* A failed backend may modify an output; the owner must not publish it. */
    *out=resume_result==ESP_OK?(void *)(uintptr_t)2:(void *)(uintptr_t)99;
    return resume_result;
}
#include "display_control_under_test.inc"
static void reset(void)
{
    s_gsp=(void *)(uintptr_t)1;
    s_app_task=current_task=(void *)(uintptr_t)11;
    s_brightness_pause=NULL;
    pauses=writes=resumes=step=write_step=resume_step=written=0;
    pause_result=write_result=resume_result=ESP_OK;
}
int main(void)
{
    reset();
    assert(mosaico_game_app_set_brightness(-1)==ESP_ERR_INVALID_ARG);
    assert(mosaico_game_app_set_brightness(101)==ESP_ERR_INVALID_ARG);
    assert(!pauses && !writes && !resumes);
    s_gsp=NULL;
    assert(mosaico_game_app_set_brightness(40)==ESP_ERR_INVALID_STATE);
    reset(); current_task=(void *)(uintptr_t)12;
    assert(mosaico_game_app_set_brightness(40)==ESP_ERR_INVALID_STATE);
    assert(!pauses && !writes && !resumes);
    reset(); pause_result=ESP_ERR_TIMEOUT;
    assert(mosaico_game_app_set_brightness(40)==ESP_ERR_TIMEOUT);
    assert(pauses==1 && !writes && !resumes && !s_brightness_pause);
    reset();
    assert(mosaico_game_app_set_brightness(73)==ESP_OK);
    assert(pauses==1 && writes==1 && resumes==1 && written==73);
    assert(write_step<resume_step && !s_brightness_pause && s_gsp==(void *)(uintptr_t)2);
    reset(); write_result=ESP_FAIL;
    assert(mosaico_game_app_set_brightness(40)==ESP_FAIL);
    assert(writes==1 && resumes==1 && !s_brightness_pause);
    reset(); resume_result=ESP_ERR_TIMEOUT;
    assert(mosaico_game_app_set_brightness(40)==ESP_ERR_TIMEOUT);
    assert(s_brightness_pause==&token && s_gsp==(void *)(uintptr_t)1);
    assert(mosaico_game_app_set_brightness(45)==ESP_ERR_TIMEOUT);
    assert(pauses==1 && writes==1 && resumes==2 && s_brightness_pause==&token);
    /* The engine loop also invokes this helper without another slider edit. */
    resume_result=ESP_OK;
    assert(resume_brightness_display()==ESP_OK);
    assert(!s_brightness_pause && s_gsp==(void *)(uintptr_t)2 && resumes==3);
    assert(mosaico_game_app_set_brightness(45)==ESP_OK);
    assert(pauses==2 && writes==2 && written==45 && resumes==4);
    puts("PASS: actual engine brightness transaction: exclusion, bounded pause, always resume, retained token and recovery");
    return 0;
}
