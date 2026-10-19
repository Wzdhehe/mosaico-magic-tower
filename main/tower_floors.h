// SPDX-License-Identifier: MIT
#pragma once
#include "tower.h"

/* Floors grow by region so early floors stay readable without a camera:
 *   region I   (1-10)   11x11  -- exactly one viewport
 *   region II  (11-20)  13x13
 *   region III (21-30)  15x15
 *   region IV  (31-40)  17x17
 *   region V   (41-50)  19x19
 * The save always reserves TOWER_MAX_W * TOWER_MAX_H bits per floor, so the
 * wire format does not depend on the shape of an individual floor. */
typedef struct {
    uint8_t w, h;
    const char *const *rows;   /* h NUL-terminated rows of w tiles */
} tower_floor_desc_t;

extern const tower_floor_desc_t tower_floor_table[TOWER_FLOORS];
