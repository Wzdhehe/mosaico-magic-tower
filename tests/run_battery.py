"""Read-only battery adapter: real C with a fault-injecting ESP-IDF I2C port."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix='battery-') as td:
        p=Path(td)
        (p/'driver').mkdir(); (p/'bsp').mkdir()
        (p/'esp_err.h').write_text('typedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_ERR_INVALID_ARG 1\n#define ESP_ERR_NO_MEM 2\n#define ESP_FAIL 3\n')
        (p/'driver/i2c_master.h').write_text('''#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef struct {int dev_addr_length; unsigned device_address,scl_speed_hz;} i2c_device_config_t;
#define I2C_ADDR_BIT_LEN_7 0
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t*,i2c_master_dev_handle_t*);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const uint8_t*,size_t,uint8_t*,size_t,int);
''')
        (p/'bsp/power.h').write_text('#include "driver/i2c_master.h"\nesp_err_t bsp_i2c_init(void);\ni2c_master_bus_handle_t bsp_i2c_get_handle(void);\n')
        (p/'test.c').write_text('''#include "game_battery.h"
#include "driver/i2c_master.h"
#include <assert.h>
#include <stdio.h>
static unsigned reads,removed; static int fail,attach_fail;
static unsigned soc=67,flags=8; static int current=25;
esp_err_t bsp_i2c_init(void){return 0;}
i2c_master_bus_handle_t bsp_i2c_get_handle(void){return (void*)1;}
esp_err_t i2c_master_bus_add_device(void*b,const i2c_device_config_t*c,void**d){
 assert(b && c->device_address==0x55 && c->scl_speed_hz==100000); if(attach_fail)return 3;*d=(void*)2;return 0;}
esp_err_t i2c_master_bus_rm_device(void*d){assert(d==(void*)2);++removed;return 0;}
esp_err_t i2c_master_transmit_receive(void*d,const uint8_t*r,size_t n,uint8_t*out,size_t len,int timeout){
 assert(d && n==1 && len==2 && timeout==10);++reads;if(fail)return 3;
 unsigned v=0;switch(*r){case 0x0a:v=flags;break;case 0x0c:v=(uint16_t)current;break;case 0x2c:v=soc;break;default:assert(0);}
 out[0]=(uint8_t)v;out[1]=(uint8_t)(v>>8);return 0;}
int main(void){
 game_battery_handle_t h=NULL;game_battery_reading_t r;
 assert(game_battery_create(NULL)!=0);attach_fail=1;assert(game_battery_create(&h)!=0 && !h);attach_fail=0;
 assert(game_battery_create(&h)==0);
 assert(game_battery_poll(h,0,&r) && r.percent==67 && r.state==GAME_BATTERY_CHARGING);
 unsigned n=reads;for(int64_t t=1;t<5000000;t+=10000)assert(!game_battery_poll(h,t,&r));assert(reads==n);
 current=-33;assert(game_battery_poll(h,5000000,&r) && r.state==GAME_BATTERY_DISCHARGING);
 flags=8|512;soc=100;current=0;assert(game_battery_poll(h,10000000,&r) && r.state==GAME_BATTERY_CHARGING);
 flags=8|256;soc=66;current=25;assert(game_battery_poll(h,15000000,&r) && r.state==GAME_BATTERY_DISCHARGING);
 flags=0;assert(game_battery_poll(h,20000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 flags=8;soc=101;assert(game_battery_poll(h,25000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 soc=60;fail=1;assert(game_battery_poll(h,30000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 fail=0;assert(game_battery_poll(h,35000000,&r) && r.state==GAME_BATTERY_CHARGING && r.percent==60);
 game_battery_delete(h);game_battery_delete(NULL);assert(removed==1);
 puts("PASS: read-only register whitelist, charging/full/presence/errors, five-second polling and cleanup");return 0;}
''')
        binary=p/'battery.exe'
        subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic',
                        '-I',str(p),'-I',str(ROOT/'main'),str(p/'test.c'),str(ROOT/'main/game_battery.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':main()
