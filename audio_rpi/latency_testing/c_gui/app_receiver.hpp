#pragma once
/*
 * app_receiver.hpp - TCP server on port 5000 that receives JSON preset
 *                    updates from the Flutter/Android mobile app.
 *
 * Mirrors the Python TcpServer (Interfaz/server/receiver_app.py):
 *   - Binds to 0.0.0.0:5000
 *   - Accepts one client at a time
 *   - Reads newline-delimited JSON
 *   - Calls the registered PresetCallback for each valid JSON object
 */

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class AppReceiver {
public:
    using PresetCallback = std::function<void(const json& preset_data)>;

    explicit AppReceiver(int port = 5000);
    ~AppReceiver();

    /* Start the background listener thread. */
    void start(PresetCallback cb);

    /* Signal the thread to stop and wait for it to exit. */
    void stop();

    bool is_running() const { return running_.load(); }

private:
    int                  port_;
    std::atomic<bool>    running_{false};
    std::thread          thread_;
    PresetCallback       callback_;

    void run();
};
