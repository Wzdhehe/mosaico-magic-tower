"""Initialization retry, nonblocking UI and safe handle handoff on real threads."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    cc=os.environ.get('CC') or shutil.which('gcc') or 'D:/espbulid/mingw64/bin/gcc.exe'
    with tempfile.TemporaryDirectory(prefix='battery-service-') as td:
        p=Path(td);(p/'freertos').mkdir()
        (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL 3\n#define ESP_ERR_NO_MEM 2\n#define ESP_ERR_INVALID_ARG 1\n')
        (p/'esp_log.h').write_text('#include <stdio.h>\n#define ESP_LOGI(tag,...) do{(void)(tag);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n')
        (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <stdint.h>\ntypedef int BaseType_t;typedef uint32_t TickType_t;\n#define pdPASS 1\n#define pdTRUE 1\n#define portMAX_DELAY UINT32_MAX\n#define pdMS_TO_TICKS(x) (x)\n')
        (p/'freertos/task.h').write_text('#pragma once\n#include "FreeRTOS.h"\ntypedef void *TaskHandle_t;typedef void(*TaskFunction_t)(void*);\nBaseType_t xTaskCreate(TaskFunction_t,const char*,unsigned,void*,unsigned,TaskHandle_t*);\nvoid xTaskNotifyGive(TaskHandle_t);unsigned ulTaskNotifyTake(BaseType_t,TickType_t);void vTaskDelete(TaskHandle_t);\n')
        (p/'freertos/queue.h').write_text('#pragma once\n#include "FreeRTOS.h"\ntypedef void *QueueHandle_t;\nQueueHandle_t xQueueCreate(unsigned,unsigned);BaseType_t xQueueSend(QueueHandle_t,const void*,TickType_t);BaseType_t xQueueReceive(QueueHandle_t,void*,TickType_t);void vQueueDelete(QueueHandle_t);\n')
        (p/'freertos/semphr.h').write_text('#pragma once\n#include "FreeRTOS.h"\ntypedef void *SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateBinary(void);BaseType_t xSemaphoreGive(SemaphoreHandle_t);BaseType_t xSemaphoreTake(SemaphoreHandle_t,TickType_t);void vSemaphoreDelete(SemaphoreHandle_t);\n')
        exe=p/'test.exe'
        subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I'+str(p),'-I'+str(ROOT/'main'),str(ROOT/'tests/test_battery_service.c'),str(ROOT/'main/game_battery_service.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=10)
    print('PASS: background battery initialization retries, UI stays responsive, ownership and shutdown')

if __name__=='__main__':main()
