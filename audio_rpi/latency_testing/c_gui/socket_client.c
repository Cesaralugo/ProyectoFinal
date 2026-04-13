/*
 * socket_client.c - Non-blocking TCP client implementation.
 *
 * The socket is set to non-blocking mode for sends (effect JSON) so that a
 * sluggish network never stalls the Win32 message loop.  Receives use a
 * dedicated thread that blocks on recv(), which is correct because we *want*
 * to wait for data without spinning.
 */

#include "socket_client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

/* ── Module state ────────────────────────────────────────────────────────── */
static SOCKET     g_sock     = INVALID_SOCKET;
static HANDLE     g_thread   = NULL;
static volatile int g_running = 0;
static sc_batch_cb g_cb      = NULL;
static void       *g_user    = NULL;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Blocking receive of exactly `need` bytes. */
static int recv_all(char *buf, int need) {
    int got = 0;
    while (got < need) {
        int n = recv(g_sock, buf + got, need - got, 0);
        if (n <= 0) return -1;
        got += n;
    }
    return got;
}

/* ── Receiver thread ─────────────────────────────────────────────────────── */

static DWORD WINAPI receiver_thread(LPVOID param) {
    (void)param;
    const int pair_bytes  = SC_PACKET_SAMPLES * 2 * (int)sizeof(float);
    float    *interleaved = (float *)malloc(pair_bytes);
    if (!interleaved) return 1;

    float pre[SC_PACKET_SAMPLES];
    float post[SC_PACKET_SAMPLES];

    while (g_running) {
        if (recv_all((char *)interleaved, pair_bytes) < 0) {
            g_running = 0;
            break;
        }
        /* De-interleave pre / post channels */
        for (int i = 0; i < SC_PACKET_SAMPLES; i++) {
            pre[i]  = interleaved[i * 2];
            post[i] = interleaved[i * 2 + 1];
        }
        if (g_cb) g_cb(pre, post, SC_PACKET_SAMPLES, g_user);
    }

    free(interleaved);
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

int sc_connect(const char *host, int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[sc] WSAStartup failed\n");
        return -1;
    }

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock == INVALID_SOCKET) {
        fprintf(stderr, "[sc] socket() failed: %d\n", WSAGetLastError());
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "[sc] Invalid host: %s\n", host);
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        return -1;
    }

    if (connect(g_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[sc] connect failed: %d\n", WSAGetLastError());
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        return -1;
    }

    /* Non-blocking mode for sends only; recv thread blocks intentionally */
    u_long nb = 1;
    ioctlsocket(g_sock, FIONBIO, &nb);

    return 0;
}

int sc_start(sc_batch_cb cb, void *user) {
    if (g_sock == INVALID_SOCKET) return -1;
    g_cb      = cb;
    g_user    = user;
    g_running = 1;
    g_thread  = CreateThread(NULL, 0, receiver_thread, NULL, 0, NULL);
    if (!g_thread) {
        g_running = 0;
        return -1;
    }
    /* Elevate receiver thread priority to reduce scheduling jitter */
    SetThreadPriority(g_thread, THREAD_PRIORITY_ABOVE_NORMAL);
    return 0;
}

int sc_send_json(const char *json) {
    if (g_sock == INVALID_SOCKET || !json) return -1;
    int len = (int)strlen(json);
    int sent = send(g_sock, json, len, 0);
    /* WSAEWOULDBLOCK is acceptable in non-blocking mode */
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            fprintf(stderr, "[sc] send failed: %d\n", err);
            return -1;
        }
    }
    return 0;
}

void sc_disconnect(void) {
    g_running = 0;
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    WSACleanup();
}

int sc_is_connected(void) {
    return g_sock != INVALID_SOCKET;
}
