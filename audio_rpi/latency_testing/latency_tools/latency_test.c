/*
 * latency_test.c - Per-batch latency measurement with real-time statistics.
 *
 * Connects to the audio engine TCP socket (port 54321) and reads interleaved
 * pre/post float pairs exactly as the Python GUI does.  For each received
 * batch of LM_PACKET_SAMPLES pairs the round-trip time from the first byte
 * arriving to the last byte received is measured.  Running statistics are
 * printed every ~1 second.
 *
 * Build:
 *   gcc latency_test.c -o ../bin/latency_test.exe -lws2_32 -lm
 *
 * Usage:
 *   latency_test.exe [host] [port]
 *   latency_test.exe               -> 127.0.0.1:54321
 *   latency_test.exe 192.168.1.10  -> 192.168.1.10:54321
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

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Receive exactly `need` bytes, blocking until done or error. */
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
    const char *host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int         port = (argc >= 3) ? atoi(argv[2]) : LM_TCP_PORT;

    /* ── Winsock init ────────────────────────────────────────────────────── */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[latency_test] WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "[latency_test] socket() failed: %d\n",
                WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    printf("[latency_test] Connecting to %s:%d ...\n", host, port);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[latency_test] connect failed: %d\n",
                WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printf("[latency_test] Connected. Reading batches of %d samples...\n",
           LM_PACKET_SAMPLES);
    printf("[latency_test] Packet window: %d us at %d Hz\n",
           LM_PACKET_DURATION_US, LM_SAMPLE_RATE);
    printf("%-10s %-10s %-10s %-10s %-10s %-8s\n",
           "Batches", "Min(us)", "Max(us)", "Avg(us)", "Jitter", "Overruns");
    printf("--------------------------------------------------------------\n");

    /* ── Timing setup ────────────────────────────────────────────────────── */
    lm_clock_t  clk;
    lm_stats_t  stats;
    lm_clock_init(&clk);
    lm_stats_init(&stats);

    /* One batch = 2 floats * LM_PACKET_SAMPLES (interleaved pre/post) */
    const int batch_bytes = LM_PACKET_SAMPLES * 2 * (int)sizeof(float);
    char     *buf         = (char *)malloc(batch_bytes);
    if (!buf) { fprintf(stderr, "OOM\n"); return 1; }

    double   report_at_us = lm_clock_us(&clk) + 1e6; /* print every 1 s */
    uint64_t batch_num    = 0;

    while (1) {
        /* Measure time to receive a full batch */
        LARGE_INTEGER t0 = lm_now();
        int got = recv_all(sock, buf, batch_bytes);
        LARGE_INTEGER t1 = lm_now();

        if (got <= 0) {
            printf("\n[latency_test] Connection closed by engine.\n");
            break;
        }

        double lat_us = lm_interval_us(&clk, t0, t1);
        lm_stats_update(&stats, lat_us);
        batch_num++;

        double now_us = lm_clock_us(&clk);
        if (now_us >= report_at_us) {
            printf("%-10llu %-10.1f %-10.1f %-10.1f %-10.1f %-8llu\n",
                   (unsigned long long)batch_num,
                   stats.min_us,
                   stats.max_us,
                   lm_stats_avg(&stats),
                   lm_stats_stddev(&stats),
                   (unsigned long long)stats.overruns);

            /* Reset per-second stats */
            lm_stats_init(&stats);
            report_at_us = now_us + 1e6;
        }
    }

    free(buf);
    closesocket(sock);
    WSACleanup();
    return 0;
}
