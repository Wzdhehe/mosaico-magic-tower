"""Compile the actual IDF removal function and exercise its mutex boundary."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    idf=Path(os.environ.get('IDF_PATH','D:/espbulid/esp-idf'))
    source=idf/'components/esp_driver_i2c/i2c_master.c'
    if not source.is_file():
        raise SystemExit('Set IDF_PATH to the patched, pinned ESP-IDF checkout')
    text=source.read_text(encoding='utf-8')
    start=text.index('esp_err_t i2c_master_bus_rm_device(')
    end=text.index('\nesp_err_t i2c_del_master_bus(',start)
    function=text[start:end]
    cc=os.environ.get('CC') or shutil.which('gcc') or 'D:/espbulid/mingw64/bin/gcc.exe'
    with tempfile.TemporaryDirectory(prefix='i2c-cleanup-') as td:
        p=Path(td)
        (p/'test.c').write_text('''#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_INVALID_STATE,I2C_STATUS_START=3,I2C_STATUS_DONE=7,portMAX_DELAY=99};
typedef struct {atomic_int status;int bus_lock_mux;void *device_list;} bus_t;
typedef bus_t *i2c_master_bus_handle_t;
typedef struct {bus_t *master_bus;} device_t;
typedef device_t *i2c_master_dev_handle_t;
typedef struct {device_t *device;} i2c_master_device_list_t;
static unsigned takes,gives;static int finish_transfer=1,locked;
static void xSemaphoreTake(int mutex,int timeout){(void)mutex;assert(timeout==99 && !locked);locked=1;++takes;}
static void xSemaphoreGive(int mutex){(void)mutex;assert(locked);locked=0;++gives;}
static int observed_status(atomic_int *status){if(locked && finish_transfer)*status=I2C_STATUS_DONE;return *status;}
#undef atomic_load
#define atomic_load(p) observed_status(p)
#define ESP_RETURN_ON_FALSE(c,e,...) do{if(!(c))return (e);}while(0)
#define ESP_LOGE(...) ((void)0)
#define SLIST_FOREACH(item,list,next) for((void)(list),item=NULL;item!=NULL;item=NULL)
#define SLIST_REMOVE(list,item,type,next) ((void)(list))
''' + function + '''
int main(void){
#ifdef _WIN32
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 bus_t bus={.status=1};device_t *d=malloc(sizeof(*d));assert(d);d->master_bus=&bus;
 /* Synchronous transfer is busy before lock; waiting for it must allow cleanup. */
 assert(i2c_master_bus_rm_device(d)==ESP_OK);
 assert(takes==1 && gives==1 && !locked);
 /* Outstanding asynchronous work still rejects removal without losing the lock. */
 finish_transfer=0;bus.status=1;d=malloc(sizeof(*d));assert(d);d->master_bus=&bus;
 assert(i2c_master_bus_rm_device(d)==ESP_ERR_INVALID_STATE);
 assert(takes==2 && gives==2 && !locked);
 bus.status=I2C_STATUS_DONE;assert(i2c_master_bus_rm_device(d)==ESP_OK);
 assert(takes==3 && gives==3 && !locked);
 assert(i2c_master_bus_rm_device(NULL)==ESP_ERR_INVALID_ARG);
 assert(takes==3 && gives==3);
 puts("PASS: actual IDF cleanup waits for sync transfer; async busy and null preserve ownership/lock");
 return 0;
}
''',encoding='utf-8')
        exe=p/'test.exe'
        subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-pedantic',str(p/'test.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=10)

if __name__=='__main__':main()
