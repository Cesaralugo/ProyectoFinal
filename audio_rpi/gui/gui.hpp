#pragma once
/*
 * gui.hpp  –  Main ImGui application class.
 *
 * AudioGui owns the render state: waveform ring buffers, the preset model,
 * the socket client (audio engine), and the app receiver (mobile app).
 *
 * Call render() once per frame from the GLFW main loop.
 * Socket callbacks run on background threads; data is transferred to the
 * render thread via a mutex-protected ring buffer.
 */

#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "app_receiver.hpp"
#include "preset_model.hpp"
#include "socket_client.hpp"

/* Number of float samples in the display ring buffer (~1 second at 44.1 kHz) */
static constexpr int WAVEFORM_RING = 44100;
/* Samples displayed per waveform panel */
static constexpr int WAVEFORM_DISPLAY = 512;

class AudioGui {
public:
    AudioGui();
    ~AudioGui();

    /* Must be called after ImGui context is initialised. */
    void init(const std::string& host = "127.0.0.1", int port = SC_AUDIO_PORT);

    /* Render one ImGui frame (call inside Begin/End frame pair). */
    void render(float dt_seconds);

    bool want_close() const { return want_close_; }

private:
    /* ── Networking ────────────────────────────────────────────────────────── */
    SocketClient client_;
    AppReceiver  app_rx_{APP_RECEIVER_PORT};
    std::string  host_{"127.0.0.1"};
    int          port_{SC_AUDIO_PORT};
    float        reconnect_timer_{0.0f};
    bool         connected_{false};

    /* ── Preset state ──────────────────────────────────────────────────────── */
    std::map<std::string, PresetModel> presets_;
    std::vector<std::string>           preset_keys_;
    std::string                        current_key_;
    int                                preset_combo_idx_{0};

    /* ── UI transient state ────────────────────────────────────────────────── */
    int  add_type_idx_{0};   // selected index in available_types()
    int  selected_fx_{-1};   // selected effect row index
    bool want_close_{false};
    char status_buf_[64]{"Connecting..."};

    /* ── Waveform ring buffer ──────────────────────────────────────────────── */
    std::array<float, WAVEFORM_RING> pre_ring_{};
    std::array<float, WAVEFORM_RING> post_ring_{};
    std::atomic<int>                 ring_write_{0};
    std::mutex                       ring_mutex_;

    /* Display-ready snapshot filled each frame */
    std::array<float, WAVEFORM_DISPLAY> disp_pre_{};
    std::array<float, WAVEFORM_DISPLAY> disp_post_{};

    /* ── Render helpers ────────────────────────────────────────────────────── */
    void render_preset_bar();
    void render_gain_slider();
    void render_effect_chain();
    void render_waveforms();
    void render_status_bar();

    /* ── Callbacks (called from background threads) ───────────────────────── */
    void on_batch(const float* pre, const float* post, int n);
    void on_remote_json(const nlohmann::json& data);

    /* ── Helpers ───────────────────────────────────────────────────────────── */
    void try_connect();
    void send_preset();
    void load_presets_file();
    void save_presets_file();
    void snap_waveform();       // copy ring → disp_pre_/disp_post_
    PresetModel& current_preset();
};
