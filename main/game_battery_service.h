// SPDX-License-Identifier: MIT
#pragma once
#include "game_battery.h"

typedef struct game_battery_service_t *game_battery_service_handle_t;
/* Start after the first healthy frame. Initialization and bounded backoff run
 * on a worker; a queue transfers the successful gauge handle to the UI owner. */
esp_err_t game_battery_service_create(game_battery_service_handle_t *out);
/* UI task only. Never waits for initialization; preserves checked-read policy. */
bool game_battery_service_poll(game_battery_service_handle_t,int64_t,game_battery_reading_t *);
/* UI task only; cooperatively waits for any in-flight initializer to finish. */
void game_battery_service_delete(game_battery_service_handle_t);
