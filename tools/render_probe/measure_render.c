/* Frame-diff measurement: how many pixels actually change between two
 * consecutive renders of the shipped renderer (main/game_view.c)?
 *
 *   gcc -std=c11 -O2 -I stub -I ../mosaico-magic-tower/main \
 *       frame_diff.c ../mosaico-magic-tower/main/game_view.c \
 *       ../mosaico-magic-tower/main/game.c ../mosaico-magic-tower/main/tower.c
 */
#include "mosaico_raylib_fast.h"
#include "game.h"
#include "game_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

gfx_stats_t g_gfx;
gfx_bucket_t g_buckets[GFX_MAX];
unsigned g_bucket_count = 0;
uint16_t g_fb[GFX_W * GFX_H];
static uint16_t g_prev[GFX_W * GFX_H];

static bool dummy_save(void *c, const uint8_t *b, size_t n) { (void)c; (void)b; (void)n; return true; }
static bool dummy_load(void *c, uint8_t *b, size_t n) { (void)c; (void)b; (void)n; return false; }

static void reset_counters(void)
{
    memset(&g_gfx, 0, sizeof g_gfx);
    g_bucket_count = 0;
    memset(g_buckets, 0, sizeof g_buckets);
}

static unsigned long diff_frames(void)
{
    unsigned long changed = 0;
    for (int i = 0; i < GFX_W * GFX_H; ++i)
        if (g_fb[i] != g_prev[i]) changed++;
    memcpy(g_prev, g_fb, sizeof g_fb);
    return changed;
}

int main(void)
{
    game_config_t cfg = {
        .width = GAME_WIDTH, .height = GAME_HEIGHT,
        .save = dummy_save, .load = dummy_load, .storage_context = NULL,
        .set_volume = NULL, .set_music = NULL
    };
    game_handle_t g = NULL;
    game_create(&cfg, &g);

    /* Enter the dungeon. */
    game_set_pointer(g, 200, 295, true);
    game_set_pointer(g, 200, 295, false);

    printf("=== 1) cost of ONE dungeon frame (nothing changes) ===\n");
    reset_counters();
    game_render(g);
    printf("  draw calls per frame   : %lu\n", g_gfx.draw_calls);
    printf("  pixel writes per frame : %lu  (%.2f full screens of %d)\n",
           g_gfx.pixel_writes, (double)g_gfx.pixel_writes / (GFX_W * GFX_H), GFX_W * GFX_H);
    printf("    of which clears       : %lu px in %lu ClearBackground calls\n",
           g_gfx.clear_pixels, g_gfx.clears);
    printf("    rects                : %lu px in %lu calls\n", g_gfx.rect_pixels, g_gfx.rects);
    printf("    fast rects (CJK runs): %lu px in %lu calls\n", g_gfx.fast_rect_pixels, g_gfx.fast_rects);
    printf("    outlines              : %lu px in %lu calls\n", g_gfx.rect_line_pixels, g_gfx.rect_lines);
    printf("    lines                 : %lu px in %lu calls\n", g_gfx.line_pixels, g_gfx.lines);
    printf("    triangles             : %lu px in %lu calls\n", g_gfx.triangle_pixels, g_gfx.triangles);
    printf("  RGB565 traffic per frame: %.2f MB\n",
           (double)g_gfx.pixel_writes * 2.0 / 1048576.0);
    printf("  at 30 fps: %.1f M px/s, %.1f MB/s, %.1f k draw-calls/s\n",
           (double)g_gfx.pixel_writes * 30 / 1e6,
           (double)g_gfx.pixel_writes * 2 * 30 / 1048576.0,
           (double)g_gfx.draw_calls * 30 / 1000.0);

    /* --- present-on-change: how many frames actually differ? --- */
    printf("\n=== 1b) present-on-change: how many frames would actually be presented? ===\n");
    {
        static uint16_t last[GFX_W * GFX_H];
        int presented = 0, skipped = 0;
        memcpy(last, g_fb, sizeof last);
        for (int i = 0; i < 300; ++i) {   /* 10 seconds at 30 fps */
            game_update(g);
            game_render(g);
            if (memcmp(last, g_fb, sizeof last) != 0) {
                presented++;
                memcpy(last, g_fb, sizeof last);
            } else {
                skipped++;
            }
        }
        printf("  over 10 s of idle play at 30 fps: %d frames changed, %d frames identical\n",
               presented, skipped);
        printf("  -> %.1f%% of the frames are byte-identical to the previous one\n",
               100.0 * skipped / 300.0);
        printf("  -> a present-on-change loop would present %d frames instead of 300\n",
               presented);
    }

    /* --- frame-to-frame diff, player idle, tick advancing --- */
    printf("\n=== 2) pixels that ACTUALLY CHANGE between consecutive frames ===\n");
    printf("    (player idle; only the tick-driven sprite bob moves)\n");
    reset_counters();
    game_render(g);
    memcpy(g_prev, g_fb, sizeof g_fb);   /* baseline */
    printf("  tick   changed px   %% of screen   draw calls   px written\n");
    for (int step = 0; step < 12; ++step) {
        game_update(g);                   /* advance the tick by 1 */
        reset_counters();
        game_render(g);
        unsigned long ch = diff_frames();
        printf("   %2d   %10lu   %8.3f%%   %10lu   %10lu\n",
               step + 1, ch, 100.0 * ch / (GFX_W * GFX_H),
               g_gfx.draw_calls, g_gfx.pixel_writes);
    }

    /* --- how long until the frame repeats? --- */
    printf("\n=== 3) animation period: tick deltas until the frame repeats ===\n");
    {
        uint16_t snapshot[GFX_W * GFX_H];
        game_update(g); game_render(g);
        memcpy(snapshot, g_fb, sizeof g_fb);
        int period = -1;
        for (int d = 1; d <= 64; ++d) {
            for (int i = 0; i < d; ++i) game_update(g);
            game_render(g);
            if (memcmp(snapshot, g_fb, sizeof g_fb) == 0) { period = d; break; }
        }
        if (period > 0)
            printf("  the picture repeats every %d ticks  ->  at 30 Hz that is %.1f Hz\n",
                   period, 30.0 / period);
        else
            printf("  the picture never repeats within 64 ticks\n");
    }

    /* --- what does a real state change cost? --- */
    printf("\n=== 4) cost of a REAL state change (one step north) ===\n");
    game_update(g);
    reset_counters();
    game_render(g);
    memcpy(g_prev, g_fb, sizeof g_fb);
    game_set_pointer(g, 64 + 5 * 32 + 16, 76 + 8 * 32 + 16, true);
    game_set_pointer(g, 64 + 5 * 32 + 16, 76 + 8 * 32 + 16, false);
    game_update(g);
    reset_counters();
    game_render(g);
    {
        unsigned long ch = diff_frames();
        printf("  changed pixels        : %lu (%.3f%% of the screen)\n",
               ch, 100.0 * ch / (GFX_W * GFX_H));
        printf("  but pixels WRITTEN    : %lu  (%.1fx more than changed)\n",
               g_gfx.pixel_writes, (double)g_gfx.pixel_writes / (double)(ch ? ch : 1));
        printf("  -> %.1f%% of the redraw is provably redundant\n",
               100.0 * (1.0 - (double)ch / (double)g_gfx.pixel_writes));
    }

    game_delete(g);
    return 0;
}
