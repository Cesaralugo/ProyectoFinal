/*
 * app_receiver.cpp  –  C++ TCP server matching Python TcpServer behaviour.
 *
 * Mirrors Interfaz/server/receiver_app.py so the Flutter app can send
 * JSON preset changes to the C++ GUI without any Python process running.
 */

#include "app_receiver.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
static void ar_close(ar_sock_t s) { closesocket(s); }
static bool ar_would_block() {
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAETIMEDOUT;
}
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <errno.h>
static void ar_close(ar_sock_t s) { ::close(s); }
static bool ar_would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }
#endif

/* ── AppReceiver ──────────────────────────────────────────────────────────── */

AppReceiver::AppReceiver(int port) : port_(port) {}

AppReceiver::~AppReceiver() { stop(); }

void AppReceiver::start(JsonCallback cb) {
    if (running_.load()) return;
    cb_      = std::move(cb);
    running_ = true;
    server_thread_ = std::thread(&AppReceiver::server_loop, this);
}

void AppReceiver::stop() {
    running_ = false;
    if (server_thread_.joinable())
        server_thread_.join();
}

/* ── Server loop (mirrors Python TcpServer.run()) ────────────────────────── */

void AppReceiver::server_loop() {
    ar_sock_t server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server == AR_INVALID) {
        fprintf(stderr, "[app_receiver] socket() failed\n");
        return;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        fprintf(stderr, "[app_receiver] bind on port %d failed\n", port_);
        ar_close(server);
        return;
    }

    ::listen(server, 5);

    /* Set accept() timeout so the loop can check running_ periodically */
#ifdef _WIN32
    DWORD tv = 1000; // ms
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{ 1, 0 };
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    fprintf(stdout, "[app_receiver] Listening on port %d\n", port_);

    while (running_.load()) {
        ar_sock_t conn = ::accept(server, nullptr, nullptr);
        if (conn == AR_INVALID) {
            if (ar_would_block()) continue; // timeout, check running_
            if (!running_.load()) break;
            fprintf(stderr, "[app_receiver] accept() error\n");
            continue;
        }
        fprintf(stdout, "[app_receiver] Client connected\n");
        handle_connection(conn);
        ar_close(conn);
        fprintf(stdout, "[app_receiver] Client disconnected\n");
    }

    ar_close(server);
}

/* ── Per-connection handler ───────────────────────────────────────────────── */

void AppReceiver::handle_connection(ar_sock_t conn) {
    /* 1-second recv timeout so we can check running_ */
#ifdef _WIN32
    DWORD tv = 1000;
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv{ 1, 0 };
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::string buffer;
    char chunk[4096];

    while (running_.load()) {
        int n = static_cast<int>(::recv(conn, chunk, sizeof(chunk) - 1, 0));
        if (n <= 0) {
            if (n == 0) break; // clean disconnect
            if (ar_would_block()) continue; // timeout
            break;
        }
        chunk[n] = '\0';
        buffer += chunk;

        /* Process all complete newline-delimited messages */
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            /* Trim whitespace */
            while (!line.empty() && (line.front() == '\r' || line.front() == ' '))
                line.erase(line.begin());
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();

            if (line.empty()) continue;

            try {
                nlohmann::json parsed = nlohmann::json::parse(line);
                fprintf(stdout, "[app_receiver] JSON received\n");
                if (cb_) cb_(parsed);
            } catch (const nlohmann::json::exception& e) {
                fprintf(stderr, "[app_receiver] JSON parse error: %s\n", e.what());
            }
        }
    }
}
