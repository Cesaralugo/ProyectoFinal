/*
 * app_receiver.cpp - TCP server for mobile-app preset delivery.
 *
 * Protocol: newline-delimited JSON, UTF-8, on TCP port 5000.
 * One client served at a time; the server loops to accept the next.
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET  sock_t;
#  define BAD_SOCK  INVALID_SOCKET
#  define CLOSE_SOCK(s) closesocket(s)
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <unistd.h>
   typedef int     sock_t;
#  define BAD_SOCK  (-1)
#  define CLOSE_SOCK(s) ::close(s)
#endif

#include "app_receiver.hpp"
#include <iostream>
#include <cstring>
#include <chrono>

/* ── Construction / destruction ──────────────────────────────────────────── */

AppReceiver::AppReceiver(int port) : port_(port) {}

AppReceiver::~AppReceiver() { stop(); }

/* ── Public API ──────────────────────────────────────────────────────────── */

void AppReceiver::start(PresetCallback cb) {
    if (running_) return;
    callback_ = std::move(cb);
    running_  = true;
    thread_   = std::thread(&AppReceiver::run, this);
}

void AppReceiver::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

/* ── Background listener ─────────────────────────────────────────────────── */

void AppReceiver::run() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == BAD_SOCK) {
        std::cerr << "[app_receiver] socket() failed\n";
        running_ = false;
        return;
    }

    /* Reuse address so restarting the app doesn't hit TIME_WAIT */
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port_);

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "[app_receiver] bind failed on port " << port_ << "\n";
        CLOSE_SOCK(srv);
        running_ = false;
        return;
    }

    listen(srv, 5);
    std::cout << "[app_receiver] Listening on port " << port_ << "\n";

    /* Non-blocking accept loop so we can check running_ */
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(srv, FIONBIO, &nb);
#else
    {
        int flags = fcntl(srv, F_GETFL, 0);
        fcntl(srv, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    while (running_) {
        sockaddr_in cli_addr{};
        socklen_t   cli_len = sizeof(cli_addr);
        sock_t cli = accept(srv, reinterpret_cast<sockaddr*>(&cli_addr), &cli_len);

        if (cli == BAD_SOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        /* Switch client to blocking with 1-second timeout */
#ifdef _WIN32
        u_long blk = 0;
        ioctlsocket(cli, FIONBIO, &blk);
        DWORD tv = 1000;
        setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
        {
            timeval tv{1, 0};
            setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        }
#endif
        std::cout << "[app_receiver] Client connected\n";

        std::string buf;
        char        tmp[4096];

        while (running_) {
            int n = recv(cli, tmp, sizeof(tmp) - 1, 0);
            if (n <= 0) break;

            tmp[n] = '\0';
            buf += tmp;

            /* Process all complete newline-delimited JSON lines */
            size_t pos;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, pos);
                buf.erase(0, pos + 1);

                /* Strip trailing \r if present */
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                try {
                    json parsed = json::parse(line);
                    std::cout << "[app_receiver] Preset received\n";
                    if (callback_) callback_(parsed);
                } catch (const std::exception& ex) {
                    std::cerr << "[app_receiver] JSON parse error: " << ex.what() << "\n";
                }
            }
        }

        CLOSE_SOCK(cli);
        std::cout << "[app_receiver] Client disconnected\n";
    }

    CLOSE_SOCK(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    running_ = false;
}
