// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* context is a NUL-terminated project-local save path, owned by the Host adapter. */
bool game_host_save(void *context, const uint8_t *bytes, size_t size);
bool game_host_load(void *context, uint8_t *bytes, size_t size);
