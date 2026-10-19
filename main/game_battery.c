// SPDX-License-Identifier: MIT
#include <stdlib.h>
#include "game_battery.h"
#include "bsp/power.h"
#include "bsp/battery.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Initialize through the pinned BSP, as the reference firmware does. Its
 * verified RAM profile is essential: SOC alone is not a calibrated gauge.
 * No application-owned unseal/reset/calibration or charger writes. Standard
 * telemetry is read with checked transactions (the BSP scalar getters return
 * zero on an I2C error, so they cannot distinguish errors from an empty cell). */
enum { GAUGE_ADDRESS=0x55, REG_STATUS=0x0a, REG_CURRENT=0x0c, REG_SOC=0x2c,
       BAT_PRESENT=1u<<3, CHARGE_INHIBIT=1u<<8,
       DISCHARGING=1u, INIT_COMPLETE=1u<<5, CONFIG_UPDATE=1u<<10,
       READ_TIMEOUT_MS=10 };
#define POLL_US INT64_C(5000000)
#define LAST_GOOD_US INT64_C(15000000)
typedef struct {
    uint16_t voltage,current,status,soc,remaining,full,design,operation;
} gauge_sample_t;
struct game_battery_t {
    i2c_master_dev_handle_t device;
    int64_t next_poll,next_log,settle_until,last_good_at;
    bool previous_valid,have_sample,have_good,holding_good;
    game_battery_state_t previous_state;
    game_battery_reading_t last_good;
    esp_err_t read_error;
    uint8_t failed_reg;
};
static bool read_sample(game_battery_handle_t,gauge_sample_t *);
static void log_sample(const char *,const gauge_sample_t *,bool);
static bool read_word(game_battery_handle_t,uint8_t,uint16_t *);
static esp_err_t identify(game_battery_handle_t h)
{
    /* Read-only DEVICE_NUMBER query at the checked reader's conservative
     * 100kHz. No security/profile/calibration state is changed. */
    const uint8_t command[]={0x00,0x01,0x00};
    for(unsigned attempt=0;attempt<3u;++attempt) {
        const esp_err_t err=i2c_master_transmit(h->device,command,sizeof(command),READ_TIMEOUT_MS);
        vTaskDelay(pdMS_TO_TICKS(20));
        uint16_t mac=0;
        const bool read=err==ESP_OK && read_word(h,0x40,&mac);
        ESP_LOGI("tower_battery","identity attempt=%u write=%d read=%d type=0x%04x",
            attempt+1u,(int)err,(int)read,(unsigned)mac);
        if(read && mac==0x0220u) return ESP_OK;
    }
    return ESP_FAIL;
}
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
    gauge_sample_t before={0};
    const bool readable=read_sample(h,&before);
    log_sample("before official init",&before,readable);
    err=identify(h);
    if(err!=ESP_OK) { game_battery_delete(h); return err; }
    /* The BSP owns its singleton gauge and borrowed bus wrapper for the app
     * lifetime. Delete below releases only our checked-read attachment. */
    err=bsp_battery_init();
    if(err!=ESP_OK) { game_battery_delete(h); return err; }
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
    const esp_err_t err=i2c_master_transmit_receive(h->device,&reg,1,bytes,2,READ_TIMEOUT_MS);
    if(err!=ESP_OK) {
        h->read_error=err;h->failed_reg=reg;
        return false;
    }
    *value=(uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1]<<8));
    return true;
}
static bool read_sample(game_battery_handle_t h,gauge_sample_t *r)
{
    return read_word(h,0x08,&r->voltage) && read_word(h,REG_STATUS,&r->status) &&
        read_word(h,REG_CURRENT,&r->current) && read_word(h,0x10,&r->remaining) &&
        read_word(h,0x12,&r->full) && read_word(h,REG_SOC,&r->soc) &&
        read_word(h,0x3a,&r->operation) && read_word(h,0x3c,&r->design);
}
static int signed_current(uint16_t word)
{ return word<=INT16_MAX?(int)word:(int)word-65536; }
static void log_sample(const char *where,const gauge_sample_t *r,bool valid)
{
    ESP_LOGI("tower_battery","%s valid=%d V=%u I=%d SOC=%u RM=%u FCC=%u DC=%u status=0x%04x op=0x%04x",
        where,(int)valid,(unsigned)r->voltage,signed_current(r->current),(unsigned)r->soc,
        (unsigned)r->remaining,(unsigned)r->full,(unsigned)r->design,
        (unsigned)r->status,(unsigned)r->operation);
}
static const char *sample_error(const gauge_sample_t *r)
{
    if(!(r->status&BAT_PRESENT)) return "battery absent";
    if(!(r->operation&INIT_COMPLETE)) return "initializing";
    if(r->operation&CONFIG_UPDATE) return "config update";
    if(r->voltage<2500u || r->voltage>4500u) return "voltage range";
    if(r->soc>100u) return "SOC range";
    if(r->design<40u || r->design>120u) return "design capacity";
    if(r->full<r->design/4u || r->full>r->design*2u) return "full capacity";
    if(r->remaining>r->full) return "remaining capacity";
    /* RM is only whole mAh on this small cell; allow rounding and the gauge's
     * one-second update boundary, but reject a torn/contradictory snapshot. */
    const unsigned derived=(100u*r->remaining+r->full-1u)/r->full;
    return derived+3u>=r->soc && r->soc+3u>=derived?NULL:"SOC capacity mismatch";
}
bool game_battery_poll(game_battery_handle_t h,int64_t now,game_battery_reading_t *out)
{
    if(!h || !out) return false;
    if(now<h->next_poll) {
        /* Expire the displayed cache on time even if UI scheduling shifted
         * the next hardware poll beyond the deadline. No I2C read needed. */
        if(h->holding_good && now-h->last_good_at>=LAST_GOOD_US) {
            h->holding_good=false;
            *out=(game_battery_reading_t){.state=GAME_BATTERY_UNKNOWN};
            return true;
        }
        return false;
    }
    if(!h->have_sample) h->settle_until=now+INT64_C(15000000);
    *out=(game_battery_reading_t){.state=GAME_BATTERY_UNKNOWN};
    gauge_sample_t r={0};
    bool valid=false,retried=false;
    /* At most one immediate retry, with a fresh complete snapshot. No sleep,
     * heap allocation or extra task in this UI-owned polling path. */
    for(unsigned attempt=0;attempt<2u;++attempt) {
        r=(gauge_sample_t){0};h->read_error=ESP_OK;h->failed_reg=0;
        const bool readable=read_sample(h,&r);
        const char *reason=readable?sample_error(&r):"I2C read";
        if(!reason) { valid=true;break; }
        ESP_LOGW("tower_battery","rejected attempt=%u reason=%s reg=0x%02x error=%d last_good_age_ms=%lld",
            attempt+1u,reason,(unsigned)h->failed_reg,(int)h->read_error,
            h->have_good?(long long)((now-h->last_good_at)/1000):-1LL);
        log_sample("rejected raw",&r,false);
        retried=true;
    }
    const int ma=signed_current(r.current);
    const game_battery_state_t state=!valid?GAME_BATTERY_UNKNOWN:
        (!(r.status&CHARGE_INHIBIT) && (ma>0 || (ma==0 && !(r.status&DISCHARGING))))?
        GAME_BATTERY_CHARGING:GAME_BATTERY_DISCHARGING;
    /* Gauge initialization can briefly expose stale standard registers after
     * its verified profile update. Recheck promptly during a bounded startup
     * window; settled operation remains one sample per five seconds. */
    h->next_poll=now+(!valid && now<h->settle_until?INT64_C(1000000):POLL_US);
    if(!h->have_sample || (retried && valid) || valid!=h->previous_valid || state!=h->previous_state || now>=h->next_log) {
        log_sample("sample",&r,valid);
        h->next_log=now+INT64_C(60000000);
    }
    h->have_sample=true;h->previous_valid=valid;h->previous_state=state;
    if(!valid) {
        /* An unsuccessful poll never refreshes the cache timestamp. At the
         * exact expiry boundary show unknown, including before any good read. */
        h->holding_good=h->have_good && now-h->last_good_at<LAST_GOOD_US;
        if(h->holding_good) *out=h->last_good;
        return true;
    }
    out->percent=r.soc; /* FC is an alarm flag, never a replacement percentage. */
    /* DSG clear denotes charging/relaxation. At actual charge completion the
     * current can be zero without FC set: do not turn a full idle cell white
     * merely because its charge-termination alarm is not configured. */
    out->state=state;
    h->last_good=*out;h->last_good_at=now;h->have_good=true;h->holding_good=false;
    return true;
}
