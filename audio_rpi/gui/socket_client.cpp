/*
 * socket_client.cpp  –  Non-blocking TCP client implementation.
 *
 * The socket is set to non-blocking mode for sends only so that
 * send_json() never stalls the UI thread.  The receiver thread
 * uses blocking recv() which is correct – we want to sleep until
 * data arrives rather than spin.
 */

#include "socket_client.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif

/* ── Platform helpers ─────────────────────────────────────────────────────── */

static void close_sock(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

static void set_nonblocking(socket_t s) {
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

/* ── SocketClient ─────────────────────────────────────────────────────────── */

SocketClient::SocketClient() = default;

SocketClient::~SocketClient() {
    disconnect();
}

bool SocketClient::connect(const std::string& host, int port) {
    if (sock_ != INVALID_SOCK)
        return true; // already connected

    socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) {
        fprintf(stderr, "[socket_client] socket() failed\n");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        fprintf(stderr, "[socket_client] inet_pton failed for %s\n", host.c_str());
        close_sock(s);
        return false;
    }

    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_sock(s);
        return false;
    }

    /* Non-blocking mode for sends; recv thread uses blocking recv() */
    set_nonblocking(s);

    sock_ = s;
    return true;
}

void SocketClient::start(BatchCallback cb) {
    if (sock_ == INVALID_SOCK || running_.load()) return;
    cb_      = std::move(cb);
    running_ = true;
    recv_thread_ = std::thread(&SocketClient::recv_loop, this);
}

bool SocketClient::send_json(const std::string& json) {
    if (sock_ == INVALID_SOCK) return false;
    std::string msg = json + "\n";
    int sent = ::send(sock_,
                      msg.c_str(),
                      static_cast<int>(msg.size()),
                      0);
    if (sent < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return true; // non-blocking, try later
        fprintf(stderr, "[socket_client] send failed: %d\n", err);
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        perror("[socket_client] send");
#endif
        return false;
    }
    return true;
}

void SocketClient::disconnect() {
    running_ = false;
    if (sock_ != INVALID_SOCK) {
        close_sock(sock_);
        sock_ = INVALID_SOCK;
    }
    if (recv_thread_.joinable())
        recv_thread_.join();
}

bool SocketClient::is_connected() const {
    return sock_ != INVALID_SOCK;
}

/* ── Private ──────────────────────────────────────────────────────────────── */

int SocketClient::recv_all(socket_t s, char* buf, int n) {
    int got = 0;
    while (got < n) {
        int r = static_cast<int>(::recv(s, buf + got, n - got, 0));
        if (r <= 0) return -1;
        got += r;
    }
    return got;
}

void SocketClient::recv_loop() {
    const int pair_bytes = SC_PACKET_SAMPLES * 2 * static_cast<int>(sizeof(float));
    auto* interleaved = static_cast<float*>(malloc(static_cast<size_t>(pair_bytes)));
    if (!interleaved) { running_ = false; return; }

    while (running_.load()) {
        if (recv_all(sock_, reinterpret_cast<char*>(interleaved), pair_bytes) < 0) {
            running_ = false;
            break;
        }
        float pre[SC_PACKET_SAMPLES];
        float post[SC_PACKET_SAMPLES];
        for (int i = 0; i < SC_PACKET_SAMPLES; i++) {
            pre[i]  = interleaved[i * 2];
            post[i] = interleaved[i * 2 + 1];
        }
        if (cb_)
            cb_(pre, post, SC_PACKET_SAMPLES);
    }

    free(interleaved);
}
