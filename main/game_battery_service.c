// SPDX-License-Identifier: MIT
#include "game_battery_service.h"
#include <stdatomic.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct game_battery_service_t {
    QueueHandle_t ready;
    SemaphoreHandle_t finished;
    TaskHandle_t worker;
    atomic_bool stopping;
    game_battery_handle_t gauge; /* UI-owned after queue handoff. */
};

static void initialize_worker(void *context)
{
    game_battery_service_handle_t s=context;
    unsigned retry_ms=2000;
    while(!atomic_load(&s->stopping)) {
        game_battery_handle_t gauge=NULL;
        const esp_err_t err=game_battery_create(&gauge);
        if(err==ESP_OK) {
            if(atomic_load(&s->stopping) || xQueueSend(s->ready,&gauge,0)!=pdTRUE)
                game_battery_delete(gauge);
            else
                ESP_LOGI("tower_battery","background initialization ready");
            break;
        }
        ESP_LOGW("tower_battery","initialization failed error=%d; retry in %u ms",
            (int)err,retry_ms);
        if(!atomic_load(&s->stopping))
            (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(retry_ms));
        retry_ms=retry_ms<15000u?retry_ms*2u:30000u;
    }
    /* Keep a valid task handle until cooperative shutdown. No periodic wake
     * or I2C access once the UI has received the initialized gauge. */
    while(!atomic_load(&s->stopping))
        (void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    (void)xSemaphoreGive(s->finished);
    /* The UI owns task retirement. Never self-delete between its stop flag
     * store and notification, which would leave it with a stale task handle.
     * No gauge, queue or service state is accessed after this acknowledgement. */
    for(;;)(void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
}

esp_err_t game_battery_service_create(game_battery_service_handle_t *out)
{
    if(!out)return ESP_ERR_INVALID_ARG;
    *out=NULL;
    game_battery_service_handle_t s=calloc(1,sizeof(*s));
    if(!s)return ESP_ERR_NO_MEM;
    atomic_init(&s->stopping,false);
    s->ready=xQueueCreate(1,sizeof(game_battery_handle_t));
    s->finished=xSemaphoreCreateBinary();
    if(!s->ready || !s->finished) {
        if(s->ready)vQueueDelete(s->ready);
        if(s->finished)vSemaphoreDelete(s->finished);
        free(s);return ESP_ERR_NO_MEM;
    }
    if(xTaskCreate(initialize_worker,"tower_gauge_init",4096,s,1,&s->worker)!=pdPASS) {
        vQueueDelete(s->ready);vSemaphoreDelete(s->finished);free(s);
        return ESP_ERR_NO_MEM;
    }
    *out=s;
    return ESP_OK;
}
bool game_battery_service_poll(game_battery_service_handle_t s,int64_t now,game_battery_reading_t *out)
{
    if(!s || !out)return false;
    if(!s->gauge)(void)xQueueReceive(s->ready,&s->gauge,0);
    return s->gauge && game_battery_poll(s->gauge,now,out);
}
void game_battery_service_delete(game_battery_service_handle_t s)
{
    if(!s)return;
    atomic_store(&s->stopping,true);
    xTaskNotifyGive(s->worker);
    (void)xSemaphoreTake(s->finished,portMAX_DELAY);
    vTaskDelete(s->worker);
    if(!s->gauge)(void)xQueueReceive(s->ready,&s->gauge,0);
    if(s->gauge)game_battery_delete(s->gauge);
    vQueueDelete(s->ready);vSemaphoreDelete(s->finished);free(s);
}
