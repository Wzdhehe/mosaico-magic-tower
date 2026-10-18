/* Counting + framebuffer stub for the Raylib-Lite graphics API used by
 * main/game_view.c.
 *
 * It renders into a real 480x480 RGB565 surface and counts every draw call
 * and pixel write, so two consecutive frames can be diffed pixel-by-pixel.
 * That answers the only question that matters for the frame-rate decision:
 * how many of the 230400 pixels actually CHANGE between two frames when the
 * player does nothing?
 *
 * This file is a measurement tool, not part of the shipped firmware. It is
 * byte-compatible with the API surface that game_view.c touches; it does not
 * rasterise the real artwork beyond the shapes the game actually draws.
 */
#ifndef MOSAICO_RAYLIB_FAST_H
#define MOSAICO_RAYLIB_FAST_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

typedef struct Color { unsigned char r, g, b, a; } Color;
typedef struct Vector2 { float x, y; } Vector2;
static const Color WHITE = {255, 255, 255, 255};

static inline Color Fade(Color c, float f)
{
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    c.r = (unsigned char)((float)c.r * f);
    c.g = (unsigned char)((float)c.g * f);
    c.b = (unsigned char)((float)c.b * f);
    c.a = (unsigned char)((float)c.a * f);
    return c;
}

#define GFX_W 480
#define GFX_H 480
#define GFX_MAX 64
typedef struct { const char *kind; unsigned long calls; unsigned long pixels; } gfx_bucket_t;
extern gfx_bucket_t g_buckets[GFX_MAX];
extern unsigned g_bucket_count;

typedef struct {
    unsigned long clears, clear_pixels;
    unsigned long rects, rect_pixels;
    unsigned long fast_rects, fast_rect_pixels;
    unsigned long rect_lines, rect_line_pixels;
    unsigned long lines, line_pixels;
    unsigned long triangles, triangle_pixels;
    unsigned long texts, text_chars;
    unsigned long fast_texts, fast_text_chars;
    unsigned long draw_calls;
    unsigned long pixel_writes;
} gfx_stats_t;
extern gfx_stats_t g_gfx;
extern uint16_t g_fb[GFX_W * GFX_H];

static inline void gfx_note(const char *kind, unsigned long pixels)
{
    for (unsigned i = 0; i < g_bucket_count; ++i) {
        if (strcmp(g_buckets[i].kind, kind) == 0) {
            g_buckets[i].calls++; g_buckets[i].pixels += pixels; return;
        }
    }
    if (g_bucket_count < GFX_MAX) {
        g_buckets[g_bucket_count].kind = kind;
        g_buckets[g_bucket_count].calls = 1;
        g_buckets[g_bucket_count].pixels = pixels;
        g_bucket_count++;
    }
}

static inline void gfx_px(int x, int y, Color c)
{
    if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H) return;
    g_fb[y * GFX_W + x] = (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
    g_gfx.pixel_writes++;
}

static inline void BeginDrawing(void) { }
static inline void EndDrawing(void) { }
static inline bool MosaicoFastFrameAvailable(void) { return true; }

static inline void ClearBackground(Color c)
{
    g_gfx.clears++;
    g_gfx.clear_pixels += (unsigned long)GFX_W * GFX_H;
    for (int y = 0; y < GFX_H; ++y)
        for (int x = 0; x < GFX_W; ++x) {
            g_fb[y * GFX_W + x] = (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
            g_gfx.pixel_writes++;
        }
}

static inline void DrawRectangle(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0) return;
    g_gfx.rects++; g_gfx.rect_pixels += (unsigned long)w * h; g_gfx.draw_calls++;
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) gfx_px(xx, yy, c);
}

static inline void DrawRectangleLines(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0) return;
    g_gfx.rect_lines++; g_gfx.draw_calls++;
    for (int xx = x; xx < x + w; ++xx) { gfx_px(xx, y, c); gfx_px(xx, y + h - 1, c); }
    for (int yy = y; yy < y + h; ++yy) { gfx_px(x, yy, c); gfx_px(x + w - 1, yy, c); }
}

static inline void DrawPixel(int x, int y, Color c) { g_gfx.draw_calls++; gfx_px(x, y, c); }

static inline void DrawLine(int x0, int y0, int x1, int y1, Color c)
{
    g_gfx.lines++; g_gfx.draw_calls++;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static inline void DrawTriangle(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    g_gfx.triangles++; g_gfx.draw_calls++;
    int minx = (int)floorf(fminf(a.x, fminf(b.x, c.x)));
    int maxx = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
    int miny = (int)floorf(fminf(a.y, fminf(b.y, c.y)));
    int maxy = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
    for (int y = miny; y <= maxy; ++y)
        for (int x = minx; x <= maxx; ++x) {
            /* barycentric */
            float v0 = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
            float v1 = (c.x - b.x) * (y - b.y) - (c.y - b.y) * (x - b.x);
            float v2 = (a.x - c.x) * (y - c.y) - (a.y - c.y) * (x - c.x);
            bool neg = (v0 < 0) || (v1 < 0) || (v2 < 0);
            bool pos = (v0 > 0) || (v1 > 0) || (v2 > 0);
            if (!(neg && pos)) gfx_px(x, y, col);
        }
}

static inline int MosaicoFastMeasureText(const char *t, int size)
{ (void)size; return (int)strlen(t) * 8; }

static inline void MosaicoFastDrawRectangle(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0) return;
    g_gfx.fast_rects++; g_gfx.fast_rect_pixels += (unsigned long)w * h; g_gfx.draw_calls++;
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) gfx_px(xx, yy, c);
}

static inline void MosaicoFastDrawText(const char *t, int x, int y, int size, Color c)
{
    (void)size;
    if (!t) return;
    g_gfx.fast_texts++; g_gfx.fast_text_chars += strlen(t); g_gfx.draw_calls++;
    for (size_t i = 0; t[i]; ++i)
        for (int yy = 0; yy < 8; ++yy) for (int xx = 0; xx < 8; ++xx)
            gfx_px(x + (int)i * 8 + xx, y + yy, c);
}

#endif
