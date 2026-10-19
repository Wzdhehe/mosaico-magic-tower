// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "game.h"
typedef struct game_battery_t *game_battery_handle_t;
typedef struct { unsigned percent; game_battery_state_t state; } game_battery_reading_t;
/* Owns only an I2C device attachment; borrows the BSP bus. UI-task use only.
 * No gauge profile, security, charger or control registers are written. */
esp_err_t game_battery_create(game_battery_handle_t *);
void game_battery_delete(game_battery_handle_t);
/* True means a new sample (possibly unknown) is available. Monotonic us. */
bool game_battery_poll(game_battery_handle_t, int64_t now_us, game_battery_reading_t *);
