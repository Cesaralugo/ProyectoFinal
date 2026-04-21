#pragma once
/*
 * app_receiver.hpp  –  C++ TCP server mirroring Interfaz/server/receiver_app.py
 *
 * Listens on port 5000 for JSON messages from the Flutter mobile app.
 * Each newline-delimited JSON message is decoded and forwarded to the GUI
 * via a std::function callback (analogous to pyqtSignal in Python).
 *
 * Wire protocol (matches Python TcpServer):
 *   - Messages are UTF-8 JSON strings separated by '\n'.
 *   - Multiple messages may arrive in a single recv() call.
 *   - Connection stays open until the client disconnects.
 */

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#  include <winsock2.h>
using ar_sock_t = SOCKET;
static const ar_sock_t AR_INVALID = INVALID_SOCKET;
#else
using ar_sock_t = int;
static const ar_sock_t AR_INVALID = -1;
#endif

static constexpr int APP_RECEIVER_PORT = 5000;

/* Invoked on the server thread for each fully-received JSON object. */
using JsonCallback = std::function<void(const nlohmann::json&)>;

class AppReceiver {
public:
    explicit AppReceiver(int port = APP_RECEIVER_PORT);
    ~AppReceiver();

    /* Launch the background accept/receive thread. */
    void start(JsonCallback cb);

    /* Signal the server thread to exit and join it. */
    void stop();

private:
    int          port_;
    std::thread  server_thread_;
    std::atomic<bool> running_{false};
    JsonCallback cb_;

    void server_loop();
    void handle_connection(ar_sock_t conn);
};
