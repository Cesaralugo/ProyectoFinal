/*
 * digipot_x9103p.c - X9103P digital potentiometer driver.
 *
 * Communicates with the X9103P via a USB-to-SPI serial adapter
 * (e.g. Arduino/Teensy running a simple two-byte bridge sketch).
 *
 * Frame: [0xD0, wiper_position]
 *
 * If the adapter is not present the functions degrade gracefully:
 * digipot_init() logs a warning and returns -1, while digipot_set_wiper()
 * still updates the cached position so the GUI reflects the value.
 */

#include "../include/digipot_x9103p.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define DIGIPOT_DEFAULT_PORT "COM3"
#define DIGIPOT_BAUD         9600

static HANDLE  g_hPort = INVALID_HANDLE_VALUE;
static uint8_t g_wiper = 0;

/* ── Public API ─────────────────────────────────────────────────────────── */

int digipot_init(const char *device_path)
{
    const char *port = (device_path && device_path[0]) ? device_path
                                                       : DIGIPOT_DEFAULT_PORT;

    /* The \\.\COMn prefix is always safe on Windows and required for
     * port numbers > 9; using it unconditionally avoids the ambiguity. */
    char port_path[32];
    snprintf(port_path, sizeof(port_path), "\\\\.\\%s", port);

    g_hPort = CreateFileA(port_path,
                          GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);

    if (g_hPort == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[digipot] Could not open %s (error %lu) – "
                        "DIGipot disabled\n", port, GetLastError());
        return -1;
    }

    /* Configure serial parameters */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(g_hPort, &dcb)) {
        fprintf(stderr, "[digipot] GetCommState failed (error %lu)\n",
                GetLastError());
        CloseHandle(g_hPort);
        g_hPort = INVALID_HANDLE_VALUE;
        return -1;
    }

    dcb.BaudRate = DIGIPOT_BAUD;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;

    if (!SetCommState(g_hPort, &dcb)) {
        fprintf(stderr, "[digipot] SetCommState failed (error %lu)\n",
                GetLastError());
        CloseHandle(g_hPort);
        g_hPort = INVALID_HANDLE_VALUE;
        return -1;
    }

    /* Short write timeout so a missing adapter does not stall the engine */
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    SetCommTimeouts(g_hPort, &timeouts);

    printf("[digipot] Opened %s at %d baud\n", port, DIGIPOT_BAUD);
    return 0;
}

int digipot_set_wiper(uint8_t position)
{
    if (g_hPort == INVALID_HANDLE_VALUE) {
        /* No hardware present – track value in software only */
        g_wiper = position;
        return 0;
    }

    /* Two-byte command frame */
    uint8_t frame[2] = {0xD0, position};
    DWORD written = 0;

    if (!WriteFile(g_hPort, frame, sizeof(frame), &written, NULL) ||
        written != sizeof(frame)) {
        fprintf(stderr, "[digipot] Write failed (error %lu)\n",
                GetLastError());
        return -1;
    }

    if (position != g_wiper)
        printf("[digipot] Wiper -> %u\n", (unsigned)position);

    g_wiper = position;
    return 0;
}

uint8_t digipot_get_wiper(void)
{
    return g_wiper;
}

void digipot_close(void)
{
    if (g_hPort != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hPort);
        g_hPort = INVALID_HANDLE_VALUE;
    }
}
