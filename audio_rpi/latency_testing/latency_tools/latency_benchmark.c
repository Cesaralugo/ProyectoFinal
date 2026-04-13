/*
 * latency_benchmark.c - Rapid latency profiling with histogram output.
 *
 * Connects to the audio engine, reads N batches as fast as possible, then
 * prints a latency histogram and summary statistics.  Useful for a quick
 * one-shot profile without watching a live display.
 *
 * Build:
 *   gcc latency_benchmark.c -o ../bin/latency_benchmark.exe -lws2_32 -lm
 *
 * Usage:
 *   latency_benchmark.exe [batches] [host] [port]
 *   latency_benchmark.exe              -> 1000 batches, 127.0.0.1:54321
 *   latency_benchmark.exe 5000         -> 5000 batches
 *   latency_benchmark.exe 5000 myhost  -> 5000 batches from myhost
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "latency_monitor.h"

#pragma comment(lib, "ws2_32.lib")

/* Comparator for qsort (ascending double) */
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

/* ── Histogram ───────────────────────────────────────────────────────────── */
#define HIST_BUCKETS  50
#define HIST_MAX_US   (LM_PACKET_DURATION_US * 2)   /* 2× packet window */

static uint64_t hist[HIST_BUCKETS];

static void hist_add(double us) {
    int b = (int)(us / HIST_MAX_US * HIST_BUCKETS);
    if (b < 0) b = 0;
    if (b >= HIST_BUCKETS) b = HIST_BUCKETS - 1;
    hist[b]++;
}

static void hist_print(uint64_t total) {
    double bucket_us = (double)HIST_MAX_US / HIST_BUCKETS;
    printf("\nLatency histogram (bucket width = %.0f us):\n", bucket_us);
    printf("%-14s %-8s %s\n", "Range (us)", "Count", "Bar");
    printf("--------------------------------------------------------------\n");
    for (int b = 0; b < HIST_BUCKETS; b++) {
        double lo = b * bucket_us;
        double hi = lo + bucket_us;
        int bar   = (int)(hist[b] * 40 / (total ? total : 1));
        printf("%6.0f-%5.0f  %-8llu ", lo, hi,
               (unsigned long long)hist[b]);
        for (int i = 0; i < bar; i++) putchar('#');
        putchar('\n');
    }
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static int recv_all(SOCKET s, char *buf, int need) {
    int got = 0;
    while (got < need) {
        int n = recv(s, buf + got, need - got, 0);
        if (n <= 0) return n == 0 ? 0 : -1;
        got += n;
    }
    return got;
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    uint64_t    target = (argc >= 2) ? (uint64_t)atoll(argv[1]) : 1000;
    const char *host   = (argc >= 3) ? argv[2] : "127.0.0.1";
    int         port   = (argc >= 4) ? atoi(argv[3]) : LM_TCP_PORT;

    /* ── Winsock init ────────────────────────────────────────────────────── */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[benchmark] WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "[benchmark] socket() failed: %d\n",
                WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    printf("[benchmark] Connecting to %s:%d ...\n", host, port);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[benchmark] connect failed: %d\n",
                WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printf("[benchmark] Connected. Profiling %llu batches...\n",
           (unsigned long long)target);

    /* ── Benchmark loop ──────────────────────────────────────────────────── */
    lm_clock_t clk;
    lm_stats_t stats;
    lm_clock_init(&clk);
    lm_stats_init(&stats);
    memset(hist, 0, sizeof(hist));

    const int batch_bytes = LM_PACKET_SAMPLES * 2 * (int)sizeof(float);
    char     *buf         = (char *)malloc(batch_bytes);
    if (!buf) { fprintf(stderr, "OOM\n"); return 1; }

    /* Percentile storage (keep all samples for P95/P99) */
    double *samples = (double *)malloc(target * sizeof(double));
    if (!samples) {
        fprintf(stderr, "[benchmark] Not enough memory for %llu samples\n",
                (unsigned long long)target);
        free(buf);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    uint64_t collected = 0;
    double   run_start = lm_clock_us(&clk);

    for (uint64_t i = 0; i < target; i++) {
        LARGE_INTEGER t0 = lm_now();
        int got = recv_all(sock, buf, batch_bytes);
        LARGE_INTEGER t1 = lm_now();

        if (got <= 0) {
            printf("\n[benchmark] Connection closed after %llu batches.\n", i);
            break;
        }

        double us = lm_interval_us(&clk, t0, t1);
        lm_stats_update(&stats, us);
        hist_add(us);
        samples[i] = us;
        collected++;

        /* Progress every 100 batches */
        if ((i + 1) % 100 == 0) {
            printf("\r  %llu / %llu batches...",
                   (unsigned long long)(i + 1),
                   (unsigned long long)target);
            fflush(stdout);
        }
    }
    double run_end = lm_clock_us(&clk);
    printf("\r  Done.                    \n");

    /* ── Sort for percentiles (qsort for O(n log n) at any sample count) ── */
    if (collected > 0)
        qsort(samples, (size_t)collected, sizeof(double), cmp_double);

    /* Linear interpolation: percentile(p) blends the two surrounding samples */
    double p50 = 0.0, p95 = 0.0, p99 = 0.0;
    if (collected >= 2) {
        double pos50 = 0.50 * (double)(collected - 1);
        double pos95 = 0.95 * (double)(collected - 1);
        double pos99 = 0.99 * (double)(collected - 1);
        uint64_t lo50 = (uint64_t)pos50, hi50 = lo50 + 1 < collected ? lo50 + 1 : lo50;
        uint64_t lo95 = (uint64_t)pos95, hi95 = lo95 + 1 < collected ? lo95 + 1 : lo95;
        uint64_t lo99 = (uint64_t)pos99, hi99 = lo99 + 1 < collected ? lo99 + 1 : lo99;
        p50 = samples[lo50] * (1.0 - (pos50 - (double)lo50)) + samples[hi50] * (pos50 - (double)lo50);
        p95 = samples[lo95] * (1.0 - (pos95 - (double)lo95)) + samples[hi95] * (pos95 - (double)lo95);
        p99 = samples[lo99] * (1.0 - (pos99 - (double)lo99)) + samples[hi99] * (pos99 - (double)lo99);
    } else if (collected == 1) {
        p50 = p95 = p99 = samples[0];
    }

    /* ── Summary ─────────────────────────────────────────────────────────── */
    double total_s = (run_end - run_start) / 1e6;
    printf("\n=== Benchmark Summary (%llu batches in %.2f s) ===\n",
           (unsigned long long)collected, total_s);
    printf("  Min       : %8.1f us\n", stats.min_us);
    printf("  Max       : %8.1f us\n", stats.max_us);
    printf("  Average   : %8.1f us\n", lm_stats_avg(&stats));
    printf("  Std dev   : %8.1f us\n", lm_stats_stddev(&stats));
    printf("  P50       : %8.1f us\n", p50);
    printf("  P95       : %8.1f us\n", p95);
    printf("  P99       : %8.1f us\n", p99);
    printf("  Overruns  : %llu / %llu (>%d us)\n",
           (unsigned long long)stats.overruns,
           (unsigned long long)collected,
           LM_PACKET_DURATION_US);
    printf("  Throughput: %.0f batches/s\n", collected / total_s);

    hist_print(collected);

    free(samples);
    free(buf);
    closesocket(sock);
    WSACleanup();
    return 0;
}
