// SPDX-License-Identifier: MIT
#pragma once
#include "game.h"

/* Official device port submission with retry after busy/rejected frames. */
bool game_render_device_pending(game_handle_t game);
