// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "game.h"
typedef struct game_battery_t *game_battery_handle_t;
typedef struct { unsigned percent; game_battery_state_t state; } game_battery_reading_t;
/* Calls the pinned official BSP battery initializer once, including its
 * verified RAM profile. Owns only a checked-read I2C attachment afterward;
 * BSP bus/gauge are borrowed app-lifetime singletons. UI-task use only.
 * No application-defined calibration, reset, OTP or charger writes. */
esp_err_t game_battery_create(game_battery_handle_t *);
void game_battery_delete(game_battery_handle_t);
/* UI-task only, monotonic us. A scheduled poll retries a rejected sample once.
 * Failed polls retain the last validated reading for <15s from its sample;
 * without one, or on expiry, return unknown. True means an output is ready. */
bool game_battery_poll(game_battery_handle_t, int64_t now_us, game_battery_reading_t *);
