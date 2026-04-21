#pragma once
/*
 * gui.hpp - Complete ImGui-based MultiFX Processor GUI.
 *
 * Feature parity with Interfaz/ui/main_window.py:
 *   - Preset management  (load / save / create / delete)
 *   - Effect chain       (add / remove / reorder / toggle, all 8 effects)
 *   - Effect parameters  (expandable sliders per-effect)
 *   - Master gain        (slider + dB display)
 *   - Waveform display   (pre/post, time domain & FFT toggle)
 *   - Bypass & pause     (toggles)
 *   - Mobile app sync    (AppReceiver on port 5000)
 *   - Audio engine conn  (non-blocking socket on port 54321, auto-reconnect)
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#include "preset_manager.hpp"
#include "fft_processor.hpp"
#include "app_receiver.hpp"

#include <imgui.h>

#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

class MainGUI {
public:
    MainGUI();
    ~MainGUI();

    /* Render one frame – called from the main loop every iteration. */
    void render();

    /* Called from the audio-engine receiver thread for each incoming batch. */
    void push_batch(const float* pre, const float* post, int n);

private:
    /* ── Visualiser ─────────────────────────────────────────────────────── */
    static constexpr int BUFFER_MAX     = 16384;
    static constexpr int DISPLAY_SAMPLES = 1024;

    std::deque<float> pre_buf_;
    std::deque<float> post_buf_;
    std::mutex        buf_mtx_;

    bool show_fft_    = false;
    bool paused_      = false;
    bool bypass_active_ = false;

    FFTProcessor fft_pre_;
    FFTProcessor fft_post_;

    /* ── Preset management ──────────────────────────────────────────────── */
    PresetManager            preset_mgr_;
    std::vector<std::string> preset_keys_;
    int                      selected_preset_idx_ = 0;
    std::string              current_preset_key_;
    Preset                   current_preset_;

    char new_preset_name_buf_[128] = {};  /* buffer for "Create preset" input */

    void reload_preset_keys();
    void load_preset(const std::string& key);
    void save_current_preset();

    /* ── Effect chain UI ────────────────────────────────────────────────── */
    std::vector<bool> effect_expanded_;   /* expanded state per slot */
    int               add_effect_combo_  = 0;

    void add_effect();
    void remove_effect(int idx);
    void move_effect_up(int idx);
    void move_effect_down(int idx);
    void toggle_effect(int idx);
    void handle_param_change(int effect_idx,
                              const std::string& param, float value);

    /* ── Audio engine socket (port 54321) ───────────────────────────────── */
#ifdef _WIN32
    SOCKET          eng_sock_  = INVALID_SOCKET;
#else
    int             eng_sock_  = -1;
#endif
    std::atomic<bool>  eng_running_{false};
    std::thread        eng_rx_thread_;
    std::thread        eng_reconnect_thread_;
    bool               eng_connected_ = false;
    std::string        status_msg_    = "Connecting to audio engine...";

    void eng_connect();
    void eng_disconnect();
    void eng_receiver_loop();
    void eng_reconnect_loop();
    void eng_send_json(const std::string& json_str);

    /* ── Mobile app receiver (port 5000) ────────────────────────────────── */
    AppReceiver app_rx_;
    std::mutex  pending_preset_mtx_;
    bool        has_pending_preset_ = false;
    json        pending_preset_data_;
    void handle_app_preset(const json& data);  /* called from receiver thread */
    void apply_pending_preset();               /* called from render thread    */

    /* ── Render helpers ─────────────────────────────────────────────────── */
    void render_left_panel();
    void render_right_panel();
    void render_preset_section();
    void render_gain_section();
    void render_effect_chain();
    void render_single_effect(int idx);
    void render_visualizer();

    void draw_waveform(ImDrawList* dl, ImVec2 pos, ImVec2 size,
                       const std::deque<float>& buf,
                       ImU32 color, bool fill = false);
    void draw_fft(ImDrawList* dl, ImVec2 pos, ImVec2 size,
                  const std::vector<float>& freqs,
                  const std::vector<float>& mags_db,
                  ImU32 color);

    /* Send current preset to the audio engine */
    void send_to_engine();
};
