#pragma once
/*
 * socket_client.hpp  –  Non-blocking TCP client for the C++ ImGui GUI.
 *
 * Connects to the audio engine on port 54321.
 * Incoming waveform data (pre/post float pairs) is read by a background
 * thread and forwarded via a callback without blocking the render loop.
 * JSON commands (apply_preset) are sent non-blocking from the UI thread.
 *
 * Wire protocol (matches audio_rpi/src/socket_server.c):
 *   - Server sends PACKET_SAMPLES * 2 floats per packet, interleaved pre/post.
 *   - Client sends JSON strings terminated with '\n'.
 */

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <winsock2.h>
using socket_t = SOCKET;
#  define INVALID_SOCK INVALID_SOCKET
#else
using socket_t = int;
#  define INVALID_SOCK (-1)
#endif

/* Samples per packet – must match PACKET_SAMPLES in audio_rpi/include/config.h */
static constexpr int SC_PACKET_SAMPLES = 128;
static constexpr int SC_AUDIO_PORT     = 54321;

/* Callback invoked on the receiver thread for every complete batch. */
using BatchCallback =
    std::function<void(const float* pre, const float* post, int n)>;

class SocketClient {
public:
    SocketClient();
    ~SocketClient();

    /* Attempt TCP connection to the audio engine.
     * Returns true on success. */
    bool connect(const std::string& host = "127.0.0.1",
                 int                port = SC_AUDIO_PORT);

    /* Start the background receiver thread.
     * cb is called on the receiver thread – keep it short. */
    void start(BatchCallback cb);

    /* Send a preset JSON string (appends '\n' delimiter). Non-blocking. */
    bool send_json(const std::string& json);

    /* Close socket and join receiver thread. */
    void disconnect();

    bool is_connected() const;

private:
    socket_t             sock_{INVALID_SOCK};
    std::thread          recv_thread_;
    std::atomic<bool>    running_{false};
    BatchCallback        cb_;

    void recv_loop();
    static int  recv_all(socket_t s, char* buf, int n);
};
