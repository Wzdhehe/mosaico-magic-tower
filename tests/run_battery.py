"""Official initialization plus checked gauge samples; fault-injecting I2C port."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix='battery-') as td:
        p=Path(td)
        (p/'driver').mkdir(); (p/'bsp').mkdir();(p/'freertos').mkdir()
        (p/'freertos/FreeRTOS.h').write_text('#define pdMS_TO_TICKS(x) (x)\n')
        (p/'freertos/task.h').write_text('static inline void vTaskDelay(unsigned t){(void)t;}\n')
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
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const uint8_t*,size_t,int);
''')
        (p/'bsp/power.h').write_text('#include "driver/i2c_master.h"\nesp_err_t bsp_i2c_init(void);\ni2c_master_bus_handle_t bsp_i2c_get_handle(void);\n')
        (p/'bsp/battery.h').write_text('#include "esp_err.h"\nesp_err_t bsp_battery_init(void);\n')
        (p/'esp_log.h').write_text('#include <stdio.h>\nvoid battery_warning(const char *,...);\n#define ESP_LOGI(tag,...) do {(void)(tag);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW(tag,...) do {(void)(tag);battery_warning(__VA_ARGS__);}while(0)\n')
        (p/'test.c').write_text('''#include "game_battery.h"
#include "driver/i2c_master.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static unsigned reads,removed,initialized,identity_writes;
static int fail,attach_fail,init_fail,identity_nacks=1,failures_remaining,torn_soc_remaining;
static unsigned failure_reg=0x2c;
static char warning[512];static unsigned warnings;
void battery_warning(const char *fmt,...){va_list args;va_start(args,fmt);
 vsnprintf(warning,sizeof(warning),fmt,args);va_end(args);++warnings;}
static unsigned soc=67,flags=8,voltage=3900,remaining=53,full=80,design=80,operation=32;
static int current=25;
esp_err_t bsp_i2c_init(void){return 0;}
esp_err_t bsp_battery_init(void){++initialized;return init_fail?3:0;}
i2c_master_bus_handle_t bsp_i2c_get_handle(void){return (void*)1;}
esp_err_t i2c_master_bus_add_device(void*b,const i2c_device_config_t*c,void**d){
 assert(b && c->device_address==0x55 && c->scl_speed_hz==100000); if(attach_fail)return 3;*d=(void*)2;return 0;}
esp_err_t i2c_master_bus_rm_device(void*d){assert(d==(void*)2);++removed;return 0;}
esp_err_t i2c_master_transmit(void*d,const uint8_t*b,size_t n,int timeout){
 assert(d && n==3 && timeout==10 && b[0]==0 && b[1]==1 && b[2]==0);++identity_writes;
 if(identity_nacks>0){--identity_nacks;return 3;}return 0;}
esp_err_t i2c_master_transmit_receive(void*d,const uint8_t*r,size_t n,uint8_t*out,size_t len,int timeout){
 assert(d && n==1 && len==2 && timeout==10);++reads;if(fail)return 3;
 if(*r==failure_reg && failures_remaining>0){--failures_remaining;return 3;}
 unsigned v=0;switch(*r){case 0x06:v=2981;break;case 0x08:v=voltage;break;
 case 0x0a:v=flags;break;case 0x0c:case 0x14:v=(uint16_t)current;break;
 case 0x10:v=remaining;break;case 0x12:v=full;break;
 case 0x2c:v=soc;if(torn_soc_remaining>0){--torn_soc_remaining;v=101;}break;
 case 0x3a:v=operation;break;case 0x3c:v=design;break;case 0x40:case 0x00:v=0x0220;break;default:assert(0);}
 out[0]=(uint8_t)v;out[1]=(uint8_t)(v>>8);return 0;}
int main(void){
 game_battery_handle_t h=NULL;game_battery_reading_t r;
 assert(game_battery_create(NULL)!=0);attach_fail=1;assert(game_battery_create(&h)!=0 && !h);attach_fail=0;
 assert(game_battery_create(&h)==0);
 assert(initialized==1); /* register reads alone cannot initialize a profile */
 assert(identity_writes==2); /* first wake/identity packet can NACK on hardware */
 assert(game_battery_poll(h,0,&r) && r.percent==67 && r.state==GAME_BATTERY_CHARGING);
 unsigned n=reads;for(int64_t t=1;t<5000000;t+=10000)assert(!game_battery_poll(h,t,&r));assert(reads==n);
 current=-33;assert(game_battery_poll(h,5000000,&r) && r.state==GAME_BATTERY_DISCHARGING);
 flags=8|1;current=19;
 assert(game_battery_poll(h,6000000,&r)==false); /* normal cadence unchanged */
 assert(game_battery_poll(h,10000000,&r) && r.state==GAME_BATTERY_CHARGING);
 /* Real plug-cycle trace: Current=+19 while the old DSG flag is still set. */
 flags=8|512;soc=9;remaining=7;current=0;
 assert(game_battery_poll(h,15000000,&r) && r.percent==9); /* stale FC must never fabricate 100 */
 flags=8|256;soc=66;remaining=52;current=25;assert(game_battery_poll(h,20000000,&r) && r.state==GAME_BATTERY_DISCHARGING);
 flags=0;assert(game_battery_poll(h,25000000,&r) && r.percent==66 && r.state==GAME_BATTERY_DISCHARGING);
 flags=8;soc=101;assert(game_battery_poll(h,30000000,&r) && r.percent==66 && r.state==GAME_BATTERY_DISCHARGING);
 soc=60;remaining=48;fail=1;assert(game_battery_poll(h,35000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 fail=0;assert(game_battery_poll(h,40000000,&r) && r.state==GAME_BATTERY_CHARGING && r.percent==60);
 full=0;assert(game_battery_poll(h,45000000,&r) && r.percent==60 && r.state==GAME_BATTERY_CHARGING);
 full=80;operation=0;assert(game_battery_poll(h,50000000,&r) && r.percent==60 && r.state==GAME_BATTERY_CHARGING);
 operation=32|1024;assert(game_battery_poll(h,55000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 operation=32;voltage=0;assert(game_battery_poll(h,60000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 voltage=4100;remaining=1;soc=99;assert(game_battery_poll(h,65000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 remaining=48;soc=60;design=3000;assert(game_battery_poll(h,70000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 design=80;flags=8;soc=100;remaining=80;current=0;
 assert(game_battery_poll(h,75000000,&r) && r.percent==100 && r.state==GAME_BATTERY_CHARGING);
 current=-33;flags=8|1;assert(game_battery_poll(h,80000000,&r) && r.state==GAME_BATTERY_DISCHARGING);
 game_battery_delete(h);game_battery_delete(NULL);assert(removed==1);
 init_fail=1;assert(game_battery_create(&h)!=0 && !h);assert(removed==2);
 identity_nacks=10;unsigned before=identity_writes;
 assert(game_battery_create(&h)!=0 && !h && identity_writes==before+3);
 assert(removed==3 && initialized==2); /* unknown identity never reaches profile init */
 identity_nacks=0;init_fail=0;operation=0;flags=8;current=0;
 assert(game_battery_create(&h)==0);
 assert(game_battery_poll(h,0,&r) && r.state==GAME_BATTERY_UNKNOWN);
 before=reads;assert(!game_battery_poll(h,999999,&r) && reads==before);
 operation=32;assert(game_battery_poll(h,1000000,&r) && r.percent==100 && r.state==GAME_BATTERY_CHARGING);
 before=reads;assert(!game_battery_poll(h,5999999,&r) && reads==before);
 assert(game_battery_poll(h,6000000,&r));game_battery_delete(h);assert(removed==4);
 /* An isolated transport glitch retries a fresh complete sample immediately. */
 flags=8|1;current=-20;soc=97;remaining=77;operation=32;
 assert(game_battery_create(&h)==0);
 failures_remaining=1;before=reads;
 assert(game_battery_poll(h,0,&r) && r.percent==97 && r.state==GAME_BATTERY_DISCHARGING);
 assert(failures_remaining==0 && reads==before+6+8);
 assert(strstr(warning,"reg=0x2c") && strstr(warning,"error=3") && strstr(warning,"reason=I2C read"));
 /* Both attempts fail: cache is allowed, with no extra idle transactions. */
 failures_remaining=2;soc=30;remaining=24;before=reads;
 assert(game_battery_poll(h,5000000,&r) && r.percent==97 && r.state==GAME_BATTERY_DISCHARGING);
 assert(failures_remaining==0 && reads==before+12);
 before=reads;assert(!game_battery_poll(h,5999999,&r) && reads==before);
 fail=1;before=reads;
 assert(game_battery_poll(h,10000000,&r) && r.percent==97 && r.state==GAME_BATTERY_DISCHARGING);
 assert(reads==before+2);
 /* Failures must not renew the cache timestamp. Expire at exactly 15s. */
 assert(game_battery_poll(h,15000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 assert(game_battery_poll(h,20000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 fail=0;soc=30;remaining=24;current=25;flags=8;
 assert(game_battery_poll(h,25000000,&r) && r.percent==30 && r.state==GAME_BATTERY_CHARGING);
 /* No upward/downward clamping: valid gauge values replace the cache. */
 soc=99;remaining=79;
 assert(game_battery_poll(h,30000000,&r) && r.percent==99 && r.state==GAME_BATTERY_CHARGING);
 /* Semantic failures receive one retry too, but never replace a valid cache. */
 soc=101;before=reads;
 assert(game_battery_poll(h,35000000,&r) && r.percent==99 && r.state==GAME_BATTERY_CHARGING);
 assert(reads==before+16);
 assert(game_battery_poll(h,40000000,&r) && r.percent==99);
 assert(game_battery_poll(h,45000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 assert(strstr(warning,"reason=SOC range"));
 soc=99;remaining=79;torn_soc_remaining=1;before=reads;
 assert(game_battery_poll(h,50000000,&r) && r.percent==99 && r.state==GAME_BATTERY_CHARGING);
 assert(reads==before+16 && torn_soc_remaining==0);
 before=warnings;
 assert(game_battery_poll(h,55000000,&r) && warnings==before);
 game_battery_delete(h);
 /* No successful sample means no invented cached zero/percentage at startup. */
 assert(game_battery_create(&h)==0);fail=1;before=reads;
 assert(game_battery_poll(h,0,&r) && r.state==GAME_BATTERY_UNKNOWN);
 assert(reads==before+2);
 before=reads;assert(!game_battery_poll(h,999999,&r) && reads==before);
 fail=0;soc=67;remaining=53;flags=8|1;current=-20;
 assert(game_battery_poll(h,1000000,&r) && r.percent==67 && r.state==GAME_BATTERY_DISCHARGING);
 fail=1;
 assert(game_battery_poll(h,6000000,&r) && r.percent==67);
 assert(game_battery_poll(h,11000000,&r) && r.percent==67);
 assert(game_battery_poll(h,16000000,&r) && r.state==GAME_BATTERY_UNKNOWN);
 game_battery_delete(h);
 /* Real UI scheduling has jitter. Expiry must not wait for a later I2C poll. */
 fail=0;assert(game_battery_create(&h)==0);
 assert(game_battery_poll(h,0,&r) && r.percent==67);
 assert(game_battery_poll(h,20000000,&r) && r.percent==67);
 fail=1;
 assert(game_battery_poll(h,25100000,&r) && r.percent==67);
 assert(game_battery_poll(h,30200000,&r) && r.percent==67);
 before=reads;
 assert(!game_battery_poll(h,34999999,&r) && reads==before);
 assert(game_battery_poll(h,35000000,&r) && r.state==GAME_BATTERY_UNKNOWN && reads==before);
 assert(!game_battery_poll(h,35000001,&r) && reads==before);
 fail=0;
 assert(game_battery_poll(h,35200000,&r) && r.percent==67 && reads==before+8);
 game_battery_delete(h);
 puts("PASS: official init, checked SOC, single retry, 15s last-good expiry, recovery, startup, bounded polling and cleanup");return 0;}
''')
        binary=p/'battery.exe'
        subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic',
                        '-I',str(p),'-I',str(ROOT/'main'),str(p/'test.c'),str(ROOT/'main/game_battery.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=='__main__':main()
