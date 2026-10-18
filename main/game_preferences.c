// SPDX-License-Identifier: MIT
#include "game_preferences.h"
#include "game_config.h"

static bool valid(const game_preferences_t *p)
{
    return p && p->lang<=GAME_LANG_EN && p->volume<=100 &&
        p->brightness>=GAME_BRIGHTNESS_MIN && p->brightness<=GAME_BRIGHTNESS_MAX;
}
bool game_preferences_encode(const game_preferences_t *value,uint8_t *bytes)
{
    if (!bytes || !valid(value)) return false;
    bytes[0]=1; bytes[1]=value->lang; bytes[2]=value->volume; bytes[3]=value->brightness;
    return true;
}
bool game_preferences_decode(const uint8_t *bytes,game_preferences_t *out)
{
    if (!bytes || !out || bytes[0]!=1) return false;
    const game_preferences_t value={bytes[1],bytes[2],bytes[3]};
    if (!valid(&value)) return false;
    *out=value;
    return true;
}
