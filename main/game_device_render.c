// SPDX-License-Identifier: MIT
#include "game_device_render.h"
#include "mosaico_game.h"

bool game_render_device_pending(game_handle_t game)
{
    if (!game) return false;
    if (!game_needs_redraw(game)) return true;
    mosaico_game_stats_t stats;
    MosaicoGameGetStats(&stats);
    uint32_t dropped = stats.dropped_frames;
    bool rendered = game_render_pending(game);
    /* EndDrawing does not return the port's submission result. Keep the last
     * operation pending if the official display port rejected this frame. */
    MosaicoGameGetStats(&stats);
    if (stats.dropped_frames != dropped) {
        game_request_redraw(game);
        return false;
    }
    return rendered;
}
