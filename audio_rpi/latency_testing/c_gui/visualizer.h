#ifndef VISUALIZER_H
#define VISUALIZER_H

/*
 * visualizer.h - Real-time waveform rendering using Win32 GDI.
 *
 * The visualizer owns a lock-free circular buffer that is written by the
 * socket receiver thread and read by WM_PAINT on the UI thread.
 * All drawing is double-buffered to eliminate flicker.
 */

#include <windows.h>

/* Number of samples kept in the ring buffer (≈ 1 second at 44100 Hz) */
#define VIZ_RING_SIZE  44100

/* Colours used for waveform rendering */
#define VIZ_COLOR_PRE   RGB(0,   200, 255)   /* cyan  – pre-effect  */
#define VIZ_COLOR_POST  RGB(255, 100,   0)   /* amber – post-effect */
#define VIZ_COLOR_BG    RGB(20,   20,  20)   /* near-black bg       */
#define VIZ_COLOR_GRID  RGB(60,   60,  60)   /* subtle grid         */

/* Opaque visualizer context */
typedef struct viz_ctx viz_ctx_t;

/* ── API ─────────────────────────────────────────────────────────────────── */

/*
 * Allocate and initialise a visualizer context.
 * hwnd - the window that will host the waveform (used for InvalidateRect).
 * Returns NULL on allocation failure.
 */
viz_ctx_t *viz_create(HWND hwnd);

/*
 * Free resources.  Safe to call with NULL.
 */
void viz_destroy(viz_ctx_t *v);

/*
 * Push a batch of pre/post samples into the ring buffers.
 * Thread-safe (called from the socket receiver thread).
 */
void viz_push(viz_ctx_t *v, const float *pre, const float *post, int n);

/*
 * Paint the waveform into the given HDC (typically from WM_PAINT).
 * rect - the client area rectangle to fill.
 */
void viz_paint(viz_ctx_t *v, HDC hdc, const RECT *rect);

#endif /* VISUALIZER_H */
