/*
 * visualizer.c - Double-buffered GDI waveform renderer.
 *
 * The ring buffers are written atomically one index at a time with
 * InterlockedExchange so the paint thread always sees a consistent snapshot
 * without a mutex (at the cost of very occasional torn frames, which is
 * acceptable for a visual display).
 */

#include "visualizer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Internal state ─────────────────────────────────────────────────────── */

struct viz_ctx {
    HWND   hwnd;

    /* Ring buffers – one entry per sample */
    float  ring_pre[VIZ_RING_SIZE];
    float  ring_post[VIZ_RING_SIZE];
    volatile LONG write_head;   /* next slot to write (mod VIZ_RING_SIZE) */

    /* Off-screen bitmap for double-buffering */
    HDC    mem_dc;
    HBITMAP mem_bmp;
    int    bmp_w;
    int    bmp_h;
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Ensure the off-screen bitmap matches the window size. */
static void ensure_bitmap(viz_ctx_t *v, HDC hdc, int w, int h) {
    if (v->mem_bmp && v->bmp_w == w && v->bmp_h == h) return;

    if (v->mem_bmp) { DeleteObject(v->mem_bmp); v->mem_bmp = NULL; }
    if (!v->mem_dc) v->mem_dc = CreateCompatibleDC(hdc);

    v->mem_bmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(v->mem_dc, v->mem_bmp);
    v->bmp_w = w;
    v->bmp_h = h;
}

/* Draw a waveform from `ring` into mem_dc. */
static void draw_waveform(viz_ctx_t *v, const float *ring,
                           LONG head, COLORREF color,
                           int w, int h, int y_base, int y_scale) {
    HPEN pen  = CreatePen(PS_SOLID, 1, color);
    HPEN old  = (HPEN)SelectObject(v->mem_dc, pen);

    int prev_x = 0;
    int prev_y = y_base;
    BOOL first = TRUE;

    for (int px = 0; px < w; px++) {
        /* Map pixel column to ring-buffer index (oldest -> newest) */
        long idx = ((long)head - w + px + VIZ_RING_SIZE) % VIZ_RING_SIZE;
        float s  = ring[idx];
        /* Clamp [-1, 1] then scale to pixel rows */
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        int y = y_base - (int)(s * y_scale);

        if (first) {
            MoveToEx(v->mem_dc, px, y, NULL);
            first = FALSE;
        } else {
            LineTo(v->mem_dc, px, y);
        }
        prev_x = px;
        prev_y = y;
    }

    SelectObject(v->mem_dc, old);
    DeleteObject(pen);
}

/* Draw a horizontal centre line. */
static void draw_center_line(viz_ctx_t *v, int y, int w) {
    HPEN pen = CreatePen(PS_DOT, 1, VIZ_COLOR_GRID);
    HPEN old = (HPEN)SelectObject(v->mem_dc, pen);
    MoveToEx(v->mem_dc, 0, y, NULL);
    LineTo(v->mem_dc, w, y);
    SelectObject(v->mem_dc, old);
    DeleteObject(pen);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

viz_ctx_t *viz_create(HWND hwnd) {
    viz_ctx_t *v = (viz_ctx_t *)calloc(1, sizeof(viz_ctx_t));
    if (!v) return NULL;
    v->hwnd       = hwnd;
    v->write_head = 0;
    return v;
}

void viz_destroy(viz_ctx_t *v) {
    if (!v) return;
    if (v->mem_bmp) DeleteObject(v->mem_bmp);
    if (v->mem_dc)  DeleteDC(v->mem_dc);
    free(v);
}

void viz_push(viz_ctx_t *v, const float *pre, const float *post, int n) {
    for (int i = 0; i < n; i++) {
        /* Increment first, then take the modulo on a local variable so that
         * the index used for writing never exceeds VIZ_RING_SIZE - 1 even
         * when multiple threads call viz_push concurrently. */
        LONG raw  = InterlockedIncrement(&v->write_head);
        LONG head = ((raw % VIZ_RING_SIZE) + VIZ_RING_SIZE) % VIZ_RING_SIZE;
        v->ring_pre[head]  = pre[i];
        v->ring_post[head] = post[i];
    }
    /* Trigger a repaint on the UI thread */
    InvalidateRect(v->hwnd, NULL, FALSE);
}

void viz_paint(viz_ctx_t *v, HDC hdc, const RECT *rect) {
    int w = rect->right  - rect->left;
    int h = rect->bottom - rect->top;
    if (w <= 0 || h <= 0) return;

    ensure_bitmap(v, hdc, w, h);

    /* Background */
    HBRUSH bg = CreateSolidBrush(VIZ_COLOR_BG);
    FillRect(v->mem_dc, rect, bg);
    DeleteObject(bg);

    LONG head = v->write_head % VIZ_RING_SIZE;
    int half  = h / 2;
    int scale = half - 4;   /* leave 4-px margin top & bottom */

    /* Top half: pre-effect; bottom half: post-effect */
    draw_center_line(v, half / 2, w);
    draw_waveform(v, v->ring_pre,  head, VIZ_COLOR_PRE,  w, h, half / 2,     scale / 2);

    draw_center_line(v, half + half / 2, w);
    draw_waveform(v, v->ring_post, head, VIZ_COLOR_POST, w, h, half + half / 2, scale / 2);

    /* Divider line between halves */
    HPEN div = CreatePen(PS_SOLID, 1, VIZ_COLOR_GRID);
    HPEN old = (HPEN)SelectObject(v->mem_dc, div);
    MoveToEx(v->mem_dc, 0, half, NULL);
    LineTo(v->mem_dc, w, half);
    SelectObject(v->mem_dc, old);
    DeleteObject(div);

    /* Blit to screen */
    BitBlt(hdc, rect->left, rect->top, w, h, v->mem_dc, 0, 0, SRCCOPY);
}
