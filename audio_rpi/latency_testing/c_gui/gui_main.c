/*
 * gui_main.c - Native Win32 GUI replacement for the Python/PyQt6 interface.
 *
 * Window layout (approximate):
 *
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │  [Add Effect ▼]  [Remove Selected]  [▲ Up]  [▼ Down]  Gain:[1.00] │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │                                                                     │
 *  │  Effect chain list box                                              │
 *  │                                                                     │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │                                                                     │
 *  │  Waveform visualizer (pre top / post bottom)                        │
 *  │                                                                     │
 *  └─────────────────────────────────────────────────────────────────────┘
 *
 * Build:
 *   gcc gui_main.c effect_manager.c visualizer.c socket_client.c \
 *       -o ../bin/gui_engine.exe -lws2_32 -lgdi32 -lcomctl32 -mwindows
 *
 * Usage:
 *   gui_engine.exe [host] [port]
 *   gui_engine.exe               -> connects to 127.0.0.1:54321
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effect_manager.h"
#include "visualizer.h"
#include "socket_client.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

/* ── Window geometry ─────────────────────────────────────────────────────── */
#define WIN_W        700
#define WIN_H        560
#define TOOLBAR_H     36
#define LIST_H       120
#define VIZ_TOP      (TOOLBAR_H + LIST_H + 4)

/* ── Control IDs ─────────────────────────────────────────────────────────── */
#define ID_BTN_ADD    101
#define ID_BTN_REMOVE 102
#define ID_BTN_UP     103
#define ID_BTN_DOWN   104
#define ID_BTN_TOGGLE 105
#define ID_LIST       106
#define ID_EDIT_GAIN  107
#define ID_BTN_APPLY  108
#define ID_TIMER_VIZ   10
#define ID_TIMER_CONN  11

/* JSON send buffer size */
#define JSON_BUF 2048

/* ── Module globals ──────────────────────────────────────────────────────── */
static eff_state_t  g_effects;
static viz_ctx_t   *g_viz     = NULL;
static HWND         g_hwnd    = NULL;
static HWND         g_list    = NULL;
static HWND         g_edit_gain = NULL;
static char         g_host[128] = "127.0.0.1";
static int          g_port      = SC_TCP_PORT;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Refresh the list-box contents from the effect chain. */
static void refresh_list(void) {
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_effects.count; i++) {
        char label[64];
        _snprintf(label, sizeof(label), "[%s] %s",
                  g_effects.chain[i].enabled ? "ON " : "OFF",
                  eff_names[g_effects.chain[i].id]);
        SendMessageA(g_list, LB_ADDSTRING, 0, (LPARAM)label);
    }
}

/* Serialise effect state and send to audio engine. */
static void send_effects(void) {
    char json[JSON_BUF];
    if (em_to_json(&g_effects, json, JSON_BUF) > 0)
        sc_send_json(json);
}

/* Batch callback – called from socket receiver thread. */
static void on_batch(const float *pre, const float *post, int n, void *user) {
    (void)user;
    if (g_viz) viz_push(g_viz, pre, post, n);
}

/* Attempt (re)connection to audio engine. */
static void try_connect(void) {
    if (sc_is_connected()) return;
    if (sc_connect(g_host, g_port) == 0) {
        sc_start(on_batch, NULL);
        SetWindowTextA(g_hwnd, "Audio Engine GUI  [connected]");
    }
}

/* ── Add-effect popup menu ───────────────────────────────────────────────── */
static void show_add_menu(HWND hwnd, HWND btn) {
    RECT r;
    GetWindowRect(btn, &r);
    HMENU menu = CreatePopupMenu();
    for (int i = 0; i < EFF_COUNT; i++)
        AppendMenuA(menu, MF_STRING, 1000 + i, eff_names[i]);
    int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTBUTTON,
                                r.left, r.bottom, 0, hwnd, NULL);
    DestroyMenu(menu);
    if (chosen >= 1000 && chosen < 1000 + EFF_COUNT) {
        em_add(&g_effects, (eff_id_t)(chosen - 1000));
        refresh_list();
        send_effects();
    }
}

/* ── WndProc ─────────────────────────────────────────────────────────────── */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        g_hwnd = hwnd;
        em_init(&g_effects);
        g_viz = viz_create(hwnd);

        /* ── Toolbar controls ── */
        int x = 4, y = 4, bh = TOOLBAR_H - 8;

        HWND btn_add = CreateWindowExA(0, "BUTTON", "Add Effect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 110, bh, hwnd, (HMENU)ID_BTN_ADD, NULL, NULL);
        x += 114;

        CreateWindowExA(0, "BUTTON", "Remove",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 80, bh, hwnd, (HMENU)ID_BTN_REMOVE, NULL, NULL);
        x += 84;

        CreateWindowExA(0, "BUTTON", "Up",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 40, bh, hwnd, (HMENU)ID_BTN_UP, NULL, NULL);
        x += 44;

        CreateWindowExA(0, "BUTTON", "Down",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 50, bh, hwnd, (HMENU)ID_BTN_DOWN, NULL, NULL);
        x += 54;

        CreateWindowExA(0, "BUTTON", "Toggle",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 60, bh, hwnd, (HMENU)ID_BTN_TOGGLE, NULL, NULL);
        x += 64;

        /* Gain label + edit + Apply */
        CreateWindowExA(0, "STATIC", "Gain:",
            WS_CHILD | WS_VISIBLE,
            x, y + 2, 38, bh - 4, hwnd, NULL, NULL, NULL);
        x += 40;

        g_edit_gain = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "1.00",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            x, y, 55, bh, hwnd, (HMENU)ID_EDIT_GAIN, NULL, NULL);
        x += 59;

        CreateWindowExA(0, "BUTTON", "Apply",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, 50, bh, hwnd, (HMENU)ID_BTN_APPLY, NULL, NULL);

        /* ── Effect chain list box ── */
        g_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
            4, TOOLBAR_H, WIN_W - 8, LIST_H,
            hwnd, (HMENU)ID_LIST, NULL, NULL);
        SendMessage(g_list, WM_SETFONT,
                    (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

        /* Timer for reconnect attempts every 2 s */
        SetTimer(hwnd, ID_TIMER_CONN, 2000, NULL);

        /* Initial connection attempt */
        try_connect();
        return 0;
    }

    case WM_TIMER:
        if (wp == ID_TIMER_CONN) try_connect();
        return 0;

    case WM_COMMAND: {
        int id  = LOWORD(wp);
        int evt = HIWORD(wp);

        if (id == ID_BTN_ADD) {
            HWND btn = (HWND)lp;
            show_add_menu(hwnd, btn);
            return 0;
        }

        if (id == ID_BTN_REMOVE) {
            int sel = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
            if (sel >= 0) { em_remove(&g_effects, sel); refresh_list(); send_effects(); }
            return 0;
        }

        if (id == ID_BTN_UP) {
            int sel = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
            if (sel > 0) {
                em_move(&g_effects, sel, sel - 1);
                refresh_list();
                SendMessage(g_list, LB_SETCURSEL, sel - 1, 0);
                send_effects();
            }
            return 0;
        }

        if (id == ID_BTN_DOWN) {
            int sel = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < g_effects.count - 1) {
                em_move(&g_effects, sel, sel + 1);
                refresh_list();
                SendMessage(g_list, LB_SETCURSEL, sel + 1, 0);
                send_effects();
            }
            return 0;
        }

        if (id == ID_BTN_TOGGLE) {
            int sel = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
            if (sel >= 0) { em_toggle(&g_effects, sel); refresh_list(); send_effects(); }
            return 0;
        }

        if (id == ID_BTN_APPLY) {
            char buf[32];
            GetWindowTextA(g_edit_gain, buf, sizeof(buf));
            float gain = (float)atof(buf);
            if (gain < 0.0f) gain = 0.0f;
            if (gain > 4.0f) gain = 4.0f;
            em_set_gain(&g_effects, gain);
            send_effects();
            return 0;
        }

        if (id == ID_LIST && evt == LBN_DBLCLK) {
            /* Double-click toggles effect on/off */
            int sel = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
            if (sel >= 0) { em_toggle(&g_effects, sel); refresh_list(); send_effects(); }
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_viz) {
            RECT viz_rect = { 0, VIZ_TOP, WIN_W, WIN_H };
            viz_paint(g_viz, hdc, &viz_rect);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        /* Resize the list box to fill the full width */
        int w = LOWORD(lp);
        if (g_list)
            SetWindowPos(g_list, NULL, 4, TOOLBAR_H, w - 8, LIST_H,
                         SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_CONN);
        sc_disconnect();
        viz_destroy(g_viz);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ── WinMain ─────────────────────────────────────────────────────────────── */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow) {
    (void)hPrev;

    /* Parse command-line: gui_engine.exe [host] [port] */
    if (lpCmd && *lpCmd) {
        char cmd[256];
        strncpy(cmd, lpCmd, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        char *tok = strtok(cmd, " \t");
        if (tok) { strncpy(g_host, tok, sizeof(g_host) - 1); tok = strtok(NULL, " \t"); }
        if (tok) g_port = atoi(tok);
    }

    InitCommonControls();

    WNDCLASSEXA wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "AudioGUI";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, "AudioGUI",
        "Audio Engine GUI  [connecting...]",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
