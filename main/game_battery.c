// SPDX-License-Identifier: MIT
#include <stdlib.h>
#include "game_battery.h"
#include "bsp/power.h"

/* BQ27220 TRM, Standard Commands 2.1: status 0x0A, signed current 0x0C,
 * SOC 0x2C. Positive current is charging; BATTPRES bit 3, CHGINH bit 8,
 * FC bit 9. Use checked read transactions rather than BSP battery_init(),
 * whose pinned implementation can reconfigure the gauge's RAM profile. */
enum { GAUGE_ADDRESS=0x55, REG_STATUS=0x0a, REG_CURRENT=0x0c, REG_SOC=0x2c,
       BAT_PRESENT=1u<<3, CHARGE_INHIBIT=1u<<8, FULL_CHARGE=1u<<9,
       READ_TIMEOUT_MS=10 };
#define POLL_US INT64_C(5000000)
struct game_battery_t {
    i2c_master_dev_handle_t device;
    int64_t next_poll;
};
esp_err_t game_battery_create(game_battery_handle_t *out)
{
    if(!out) return ESP_ERR_INVALID_ARG;
    *out=NULL;
    esp_err_t err=bsp_i2c_init();
    if(err!=ESP_OK) return err;
    game_battery_handle_t h=calloc(1,sizeof(*h));
    if(!h) return ESP_ERR_NO_MEM;
    const i2c_device_config_t config={.dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=GAUGE_ADDRESS,.scl_speed_hz=100000};
    err=i2c_master_bus_add_device(bsp_i2c_get_handle(),&config,&h->device);
    if(err!=ESP_OK) { free(h); return err; }
    *out=h;
    return ESP_OK;
}
void game_battery_delete(game_battery_handle_t h)
{
    if(!h) return;
    (void)i2c_master_bus_rm_device(h->device);
    free(h);
}
static bool read_word(game_battery_handle_t h,uint8_t reg,uint16_t *value)
{
    uint8_t bytes[2];
    if(i2c_master_transmit_receive(h->device,&reg,1,bytes,2,READ_TIMEOUT_MS)!=ESP_OK)
        return false;
    *value=(uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1]<<8));
    return true;
}
bool game_battery_poll(game_battery_handle_t h,int64_t now,game_battery_reading_t *out)
{
    if(!h || !out || now<h->next_poll) return false;
    h->next_poll=now+POLL_US;
    *out=(game_battery_reading_t){.state=GAME_BATTERY_UNKNOWN};
    uint16_t status,soc,current;
    if(!read_word(h,REG_STATUS,&status) || !(status&BAT_PRESENT) ||
       !read_word(h,REG_SOC,&soc) || soc>100u || !read_word(h,REG_CURRENT,&current)) return true;
    out->percent=soc;
    /* Convert signed two's complement explicitly, independent of host casts. */
    const int32_t ma=current<=INT16_MAX ? (int32_t)current : (int32_t)current-65536;
    if((status&FULL_CHARGE) && ma>=0) {
        out->percent=100;
        out->state=GAME_BATTERY_CHARGING;
    }
    else if(ma>0 && !(status&CHARGE_INHIBIT)) out->state=GAME_BATTERY_CHARGING;
    else out->state=GAME_BATTERY_DISCHARGING;
    return true;
}
