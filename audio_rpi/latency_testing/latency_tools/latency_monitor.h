#ifndef LATENCY_MONITOR_H
#define LATENCY_MONITOR_H

/*
 * latency_monitor.h - Shared timing utilities for latency measurement tools.
 *
 * Uses Windows QueryPerformanceCounter for high-resolution timing.
 * All time values are in microseconds (us) unless noted otherwise.
 *
 * Packet constants (must match audio_rpi/include/config.h):
 *   SAMPLE_RATE          = 44100 Hz
 *   PACKET_SAMPLES       = 128
 *   PACKET_DURATION_US   = 128 / 44100 * 1e6 ≈ 2902 us
 */

#include <windows.h>
#include <stdint.h>
#include <math.h>

/* ── Audio constants ─────────────────────────────────────────────────────── */
#define LM_SAMPLE_RATE        44100
#define LM_PACKET_SAMPLES     128
#define LM_PACKET_DURATION_US 2902   /* theoretical packet window in µs */
#define LM_TCP_PORT           54321

/* ── Timer state ─────────────────────────────────────────────────────────── */
typedef struct {
    LARGE_INTEGER freq;   /* counts per second */
    LARGE_INTEGER start;  /* reference timestamp */
} lm_clock_t;

/* Initialise clock (call once). */
static inline void lm_clock_init(lm_clock_t *c) {
    QueryPerformanceFrequency(&c->freq);
    QueryPerformanceCounter(&c->start);
}

/* Return microseconds elapsed since lm_clock_init(). */
static inline double lm_clock_us(const lm_clock_t *c) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - c->start.QuadPart)
           * 1e6 / (double)c->freq.QuadPart;
}

/* Capture a raw timestamp for interval measurement. */
static inline LARGE_INTEGER lm_now(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

/* Convert two raw timestamps to an interval in microseconds. */
static inline double lm_interval_us(const lm_clock_t *c,
                                     LARGE_INTEGER t0, LARGE_INTEGER t1) {
    return (double)(t1.QuadPart - t0.QuadPart)
           * 1e6 / (double)c->freq.QuadPart;
}

/* ── Running statistics ──────────────────────────────────────────────────── */
typedef struct {
    double   min_us;
    double   max_us;
    double   sum_us;
    double   sum_sq_us;   /* for standard deviation */
    uint64_t count;
    uint64_t overruns;    /* samples that exceeded LM_PACKET_DURATION_US */
} lm_stats_t;

static inline void lm_stats_init(lm_stats_t *s) {
    s->min_us   = 1e18;
    s->max_us   = 0.0;
    s->sum_us   = 0.0;
    s->sum_sq_us = 0.0;
    s->count    = 0;
    s->overruns = 0;
}

static inline void lm_stats_update(lm_stats_t *s, double us) {
    if (us < s->min_us) s->min_us = us;
    if (us > s->max_us) s->max_us = us;
    s->sum_us    += us;
    s->sum_sq_us += us * us;
    s->count++;
    if (us > LM_PACKET_DURATION_US) s->overruns++;
}

static inline double lm_stats_avg(const lm_stats_t *s) {
    return s->count ? s->sum_us / (double)s->count : 0.0;
}

static inline double lm_stats_stddev(const lm_stats_t *s) {
    if (s->count < 2) return 0.0;
    double mean = lm_stats_avg(s);
    double var  = s->sum_sq_us / (double)s->count - mean * mean;
    return var > 0.0 ? sqrt(var) : 0.0;
}

/* Headroom: fraction of the packet window NOT used (1.0 = all free). */
static inline double lm_headroom(double latency_us) {
    double ratio = latency_us / LM_PACKET_DURATION_US;
    return ratio < 1.0 ? 1.0 - ratio : 0.0;
}

#endif /* LATENCY_MONITOR_H */
