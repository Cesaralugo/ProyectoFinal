/*
 * gui.cpp - ImGui MultiFX Processor GUI implementation.
 *
 * All features mirror the Python PyQt6 GUI (Interfaz/ui/main_window.py).
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET  sock_t;
#  define BAD_SOCK   INVALID_SOCKET
#  define CLOSE_SOCK(s) closesocket(s)
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   typedef int     sock_t;
#  define BAD_SOCK  (-1)
#  define CLOSE_SOCK(s) ::close(s)
#endif

#include "gui.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <chrono>

/* Number of float pairs per audio batch – must match audio engine */
static constexpr int PACKET_SAMPLES = 128;
static constexpr int ENGINE_PORT    = 54321;

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* ── Construction / destruction ──────────────────────────────────────────── */

MainGUI::MainGUI() {
    /* Load presets */
    preset_mgr_.load();
    reload_preset_keys();
    if (!preset_keys_.empty()) {
        current_preset_key_ = preset_keys_[0];
        current_preset_     = preset_mgr_.get_preset(current_preset_key_);
    }
    effect_expanded_.assign(current_preset_.effects.size(), false);

    /* Start audio engine connection & reconnect loop */
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    eng_running_ = true;
    eng_reconnect_thread_ = std::thread(&MainGUI::eng_reconnect_loop, this);

    /* Start mobile app receiver */
    app_rx_.start([this](const json& data) { handle_app_preset(data); });
}

MainGUI::~MainGUI() {
    eng_running_ = false;
    eng_disconnect();
    if (eng_reconnect_thread_.joinable()) eng_reconnect_thread_.join();
    app_rx_.stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ── Top-level render ────────────────────────────────────────────────────── */

void MainGUI::render() {
    ImGuiIO& io = ImGui::GetIO();

    /* Full-screen dockspace-style window */
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoMove       |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    /* Apply any preset received from the mobile app (thread-safe) */
    apply_pending_preset();

    float panel_w = 300.0f;
    render_left_panel();
    ImGui::SameLine();
    render_right_panel();

    ImGui::End();
    (void)panel_w;
}

/* ── Left panel ──────────────────────────────────────────────────────────── */

void MainGUI::render_left_panel() {
    ImGui::BeginChild("##left", ImVec2(300, 0), false);

    ImGui::TextColored({0.3f, 0.8f, 1.0f, 1.0f}, "MultiFX Processor");
    ImGui::Separator();

    /* Connection status */
    ImVec4 sc = eng_connected_ ? ImVec4{0.2f,1.0f,0.2f,1.0f}
                               : ImVec4{1.0f,0.4f,0.4f,1.0f};
    ImGui::TextColored(sc, "%s", status_msg_.c_str());
    ImGui::Spacing();

    render_preset_section();
    ImGui::Separator();
    render_gain_section();
    ImGui::Separator();
    render_effect_chain();

    ImGui::EndChild();
}

/* ── Preset management ───────────────────────────────────────────────────── */

void MainGUI::render_preset_section() {
    ImGui::TextColored({0.9f, 0.9f, 0.4f, 1.0f}, "Presets");

    /* Dropdown */
    if (!preset_keys_.empty()) {
        const char* current_label =
            (selected_preset_idx_ < (int)preset_keys_.size())
            ? preset_keys_[selected_preset_idx_].c_str()
            : "";

        if (ImGui::BeginCombo("##preset_combo", current_label)) {
            for (int i = 0; i < (int)preset_keys_.size(); ++i) {
                bool sel = (i == selected_preset_idx_);
                if (ImGui::Selectable(preset_keys_[i].c_str(), sel)) {
                    save_current_preset();
                    selected_preset_idx_ = i;
                    current_preset_key_  = preset_keys_[i];
                    load_preset(current_preset_key_);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    /* Save button */
    if (ImGui::Button("Save Preset")) {
        save_current_preset();
        preset_mgr_.save();
    }
    ImGui::SameLine();

    /* Delete button */
    if (ImGui::Button("Delete") && preset_keys_.size() > 1) {
        preset_mgr_.delete_preset(current_preset_key_);
        preset_mgr_.save();
        reload_preset_keys();
        selected_preset_idx_ = 0;
        if (!preset_keys_.empty()) {
            current_preset_key_ = preset_keys_[0];
            load_preset(current_preset_key_);
        }
    }

    /* Create new preset */
    ImGui::InputText("##new_name", new_preset_name_buf_,
                     sizeof(new_preset_name_buf_));
    ImGui::SameLine();
    if (ImGui::Button("New") && new_preset_name_buf_[0] != '\0') {
        std::string key = new_preset_name_buf_;
        preset_mgr_.create_preset(key, key);
        preset_mgr_.save();
        reload_preset_keys();

        /* Select the new preset */
        auto it = std::find(preset_keys_.begin(), preset_keys_.end(), key);
        if (it != preset_keys_.end()) {
            selected_preset_idx_ = (int)(it - preset_keys_.begin());
            current_preset_key_  = key;
            load_preset(key);
        }
        new_preset_name_buf_[0] = '\0';
    }
}

/* ── Master Gain ─────────────────────────────────────────────────────────── */

void MainGUI::render_gain_section() {
    ImGui::TextColored({0.9f, 0.9f, 0.4f, 1.0f}, "Master Gain");

    float gain = current_preset_.master_gain;
    float db   = (gain > 0.0f)
                 ? 20.0f * std::log10(gain)
                 : -std::numeric_limits<float>::infinity();
    char db_buf[64];
    if (std::isinf(db)) std::snprintf(db_buf, sizeof(db_buf), "-- dB");
    else std::snprintf(db_buf, sizeof(db_buf), "%.2f  (%+.1f dB)", gain, db);
    ImGui::Text("%s", db_buf);

    if (ImGui::SliderFloat("##gain", &gain, 0.1f, 4.0f, "%.2f")) {
        current_preset_.master_gain = gain;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        save_current_preset();
        send_to_engine();
    }
}

/* ── Effect chain ────────────────────────────────────────────────────────── */

void MainGUI::render_effect_chain() {
    ImGui::TextColored({0.9f, 0.9f, 0.4f, 1.0f}, "Effect Chain");

    /* Add-effect combo + button */
    const auto& avail = PresetManager::AVAILABLE_EFFECTS;    const char* combo_label = avail[add_effect_combo_].c_str();
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("##add_combo", combo_label)) {
        for (int i = 0; i < (int)avail.size(); ++i) {
            if (ImGui::Selectable(avail[i].c_str(), add_effect_combo_ == i))
                add_effect_combo_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add")) add_effect();

    /* Bypass / Pause buttons */
    ImGui::Spacing();
    if (bypass_active_) {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4{0.6f, 0.1f, 0.1f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4{0.8f, 0.2f, 0.2f, 1.0f});
    }
    if (ImGui::Button(bypass_active_ ? "BYPASS [ON]" : "BYPASS")) {
        bypass_active_ = !bypass_active_;
        for (auto& e : current_preset_.effects)
            e.enabled = !bypass_active_;
        send_to_engine();
    }
    if (bypass_active_) ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button(paused_ ? "PAUSED  [resume]" : "WATCHING")) {
        paused_ = !paused_;
    }

    ImGui::Spacing();

    /* Scrollable effect list */
    float list_h = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##effects_list", ImVec2(0, list_h), true);

    for (int i = 0; i < (int)current_preset_.effects.size(); ++i)
        render_single_effect(i);

    ImGui::EndChild();
}

/* ── Single effect widget ────────────────────────────────────────────────── */

void MainGUI::render_single_effect(int idx) {
    auto& ef = current_preset_.effects[idx];

    ImGui::PushID(idx);

    /* Header row: toggle ▶/▼, effect name, enable checkbox, up/dn, delete */
    ImGui::Separator();

    /* Expand/collapse button */
    const char* expand_lbl = effect_expanded_[idx] ? "v" : ">";
    if (ImGui::SmallButton(expand_lbl))
        effect_expanded_[idx] = !effect_expanded_[idx];
    ImGui::SameLine();

    /* Enable/disable checkbox */
    bool enabled = ef.enabled;
    if (ImGui::Checkbox("##en", &enabled)) {
        ef.enabled = enabled;
        send_to_engine();
    }
    ImGui::SameLine();

    /* Effect type label */
    ImGui::TextColored(ef.enabled ? ImVec4{0.3f,0.9f,0.4f,1.0f}
                                  : ImVec4{0.5f,0.5f,0.5f,1.0f},
                       "%s", ef.type.c_str());

    /* Right-side controls */
    float line_end = ImGui::GetContentRegionMax().x;
    float del_w    = 22.0f, mv_w = 22.0f, gap = 3.0f;
    float ctrl_x   = line_end - del_w - (mv_w + gap) * 2;
    ImGui::SameLine(ctrl_x);

    if (ImGui::ArrowButton("##up", ImGuiDir_Up))   move_effect_up(idx);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##dn", ImGuiDir_Down)) move_effect_down(idx);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.6f,0.1f,0.1f,1.0f});
    if (ImGui::SmallButton("X")) remove_effect(idx);
    ImGui::PopStyleColor();

    /* Parameter sliders (shown when expanded) */
    if (effect_expanded_[idx]) {
        for (auto& [pname, pval] : ef.params) {
            ParamRange rng = PresetManager::param_range(pname);
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "%s: %.2f %s",
                          pname.c_str(), pval, rng.unit.c_str());
            ImGui::TextUnformatted(lbl);

            ImGui::SetNextItemWidth(-1);
            std::string slider_id = "##s_" + pname;
            float tmp = pval;
            if (ImGui::SliderFloat(slider_id.c_str(), &tmp,
                                   rng.min_val, rng.max_val, "%.3f")) {
                pval = tmp;   /* update via reference while dragging */
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                pval = tmp;   /* ensure final value is committed */
                save_current_preset();
                send_to_engine();
            }
        }
        ImGui::Spacing();
    }

    ImGui::PopID();
}

/* ── Right panel ─────────────────────────────────────────────────────────── */

void MainGUI::render_right_panel() {
    ImGui::BeginChild("##right", ImVec2(0, 0), false);
    render_visualizer();
    ImGui::EndChild();
}

/* ── Visualizer ──────────────────────────────────────────────────────────── */

void MainGUI::render_visualizer() {
    float avail_h   = ImGui::GetContentRegionAvail().y;
    float wave_h    = (avail_h - 40.0f) / 2.0f;
    if (wave_h < 60.0f) wave_h = 60.0f;

    /* FFT/Time toggle */
    if (ImGui::Button(show_fft_ ? "Show Time" : "Show FFT")) {
        show_fft_ = !show_fft_;
    }
    ImGui::Spacing();

    /* Grab a snapshot of the buffers (lock briefly) */
    std::deque<float> pre_snap, post_snap;
    {
        std::lock_guard<std::mutex> lk(buf_mtx_);
        pre_snap  = pre_buf_;
        post_snap = post_buf_;
    }

    auto draw_panel = [&](const char* id, const char* title,
                          const std::deque<float>& buf,
                          FFTProcessor& fft_proc,
                          ImU32 color_wave,
                          bool is_post)
    {
        ImVec2 panel_size{ImGui::GetContentRegionAvail().x, wave_h};
        ImGui::BeginChild(id, panel_size, true,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("%s", title);

        ImVec2 pos  = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 2 || size.y < 2) { ImGui::EndChild(); return; }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos,
                          {pos.x + size.x, pos.y + size.y},
                          IM_COL32(18, 18, 26, 255));

        /* Grid centre line */
        float mid_y = pos.y + size.y * 0.5f;
        dl->AddLine({pos.x, mid_y}, {pos.x + size.x, mid_y},
                    IM_COL32(60, 60, 80, 128));

        if (!show_fft_) {
            draw_waveform(dl, pos, size, buf, color_wave);
        } else {
            std::vector<float> freqs, mags_db;
            fft_proc.compute(buf, freqs, mags_db);
            draw_fft(dl, pos, size, freqs, mags_db, color_wave);
        }

        ImGui::Dummy(size);
        ImGui::EndChild();
        (void)is_post;
    };

    /* Pre-effect */
    draw_panel("##pre_panel", "Pre Effect",
               pre_snap, fft_pre_,
               IM_COL32(0, 180, 255, 255), false);

    /* Post-effect (fall back to pre if no effects loaded) */
    const std::deque<float>& post_src =
        current_preset_.effects.empty() ? pre_snap : post_snap;
    draw_panel("##post_panel", "Post Effect",
               post_src, fft_post_,
               IM_COL32(0, 180, 255, 255), true);
}

/* ── Waveform drawing helper ─────────────────────────────────────────────── */

void MainGUI::draw_waveform(ImDrawList* dl, ImVec2 pos, ImVec2 size,
                             const std::deque<float>& buf,
                             ImU32 color, bool /*fill*/) {
    int n = std::min((int)buf.size(), DISPLAY_SAMPLES);
    if (n < 2) return;

    float mid_y = pos.y + size.y * 0.5f;
    float scale = size.y * 0.45f;

    /* Compute mean for DC removal */
    double mean = 0.0;
    for (int i = 0; i < n; ++i)
        mean += buf[buf.size() - n + i];
    mean /= n;

    std::vector<ImVec2> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
        float s = buf[buf.size() - n + i] - (float)mean;
        s = std::max(-1.0f, std::min(1.0f, s));
        float x = pos.x + (float)i / (float)(n - 1) * size.x;
        float y = mid_y - s * scale;
        pts.push_back({x, y});
    }
    dl->AddPolyline(pts.data(), (int)pts.size(), color, 0, 1.5f);
}

/* ── FFT drawing helper ──────────────────────────────────────────────────── */

void MainGUI::draw_fft(ImDrawList* dl, ImVec2 pos, ImVec2 size,
                        const std::vector<float>& freqs,
                        const std::vector<float>& mags_db,
                        ImU32 color) {
    if (freqs.empty()) return;

    const float X_MIN = 0.0f,    X_MAX = 20000.0f;
    const float Y_MIN = -170.0f, Y_MAX = 0.0f;

    std::vector<ImVec2> pts;
    pts.reserve(freqs.size());

    for (int i = 0; i < (int)freqs.size(); ++i) {
        if (freqs[i] > X_MAX) break;

        float x = pos.x + (freqs[i] - X_MIN) / (X_MAX - X_MIN) * size.x;
        float t = 1.0f - (mags_db[i] - Y_MIN) / (Y_MAX - Y_MIN);
        t = std::max(0.0f, std::min(1.0f, t));
        float y = pos.y + t * size.y;
        pts.push_back({x, y});
    }

    if ((int)pts.size() >= 2)
        dl->AddPolyline(pts.data(), (int)pts.size(), color, 0, 1.0f);

    /* Fill under curve */
    if ((int)pts.size() >= 2) {
        ImU32 fill_col = (color & 0x00FFFFFFu) | 0x3C000000u;
        ImVec2 bl{pts.front().x, pos.y + size.y};
        ImVec2 br{pts.back().x,  pos.y + size.y};
        std::vector<ImVec2> fill;
        fill.reserve(pts.size() + 2);
        fill.push_back(bl);
        for (auto& p : pts) fill.push_back(p);
        fill.push_back(br);
        dl->AddConvexPolyFilled(fill.data(), (int)fill.size(), fill_col);
    }
}

/* ── Preset helpers ──────────────────────────────────────────────────────── */

void MainGUI::reload_preset_keys() {
    preset_keys_ = preset_mgr_.preset_keys();
}

void MainGUI::load_preset(const std::string& key) {
    current_preset_ = preset_mgr_.get_preset(key);
    effect_expanded_.assign(current_preset_.effects.size(), false);
    {
        std::lock_guard<std::mutex> lk(buf_mtx_);
        pre_buf_.clear();
        post_buf_.clear();
    }
    send_to_engine();
}

void MainGUI::save_current_preset() {
    preset_mgr_.set_preset(current_preset_key_, current_preset_);
    preset_mgr_.save();
}

/* ── Effect chain helpers ────────────────────────────────────────────────── */

void MainGUI::add_effect() {
    if ((int)current_preset_.effects.size() >= PresetManager::MAX_EFFECTS) return;

    const std::string& type = PresetManager::AVAILABLE_EFFECTS[add_effect_combo_];

    /* Each type can appear only once */
    for (const auto& e : current_preset_.effects)
        if (e.type == type) return;

    int  new_idx = (int)current_preset_.effects.size() + 1;
    Effect ef;
    ef.id      = "fx_" + std::to_string(new_idx);
    ef.type    = type;
    ef.enabled = true;
    ef.params  = PresetManager::default_params(type);
    current_preset_.effects.push_back(ef);
    effect_expanded_.push_back(false);

    save_current_preset();
    send_to_engine();
}

void MainGUI::remove_effect(int idx) {
    if (idx < 0 || idx >= (int)current_preset_.effects.size()) return;
    current_preset_.effects.erase(current_preset_.effects.begin() + idx);
    effect_expanded_.erase(effect_expanded_.begin() + idx);
    {
        std::lock_guard<std::mutex> lk(buf_mtx_);
        pre_buf_.clear();
        post_buf_.clear();
    }
    save_current_preset();
    send_to_engine();
}

void MainGUI::move_effect_up(int idx) {
    if (idx <= 0 || idx >= (int)current_preset_.effects.size()) return;
    std::swap(current_preset_.effects[idx], current_preset_.effects[idx - 1]);
    std::swap(effect_expanded_[idx], effect_expanded_[idx - 1]);
    save_current_preset();
    send_to_engine();
}

void MainGUI::move_effect_down(int idx) {
    if (idx < 0 || idx >= (int)current_preset_.effects.size() - 1) return;
    std::swap(current_preset_.effects[idx], current_preset_.effects[idx + 1]);
    std::swap(effect_expanded_[idx], effect_expanded_[idx + 1]);
    save_current_preset();
    send_to_engine();
}

void MainGUI::toggle_effect(int idx) {
    if (idx < 0 || idx >= (int)current_preset_.effects.size()) return;
    current_preset_.effects[idx].enabled ^= true;
    send_to_engine();
}

/* ── Audio engine socket ─────────────────────────────────────────────────── */

void MainGUI::eng_connect() {
    if (eng_sock_ != BAD_SOCK) return;

    /* Join any previous receiver thread before starting a new one */
    if (eng_rx_thread_.joinable()) eng_rx_thread_.join();

    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == BAD_SOCK) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ENGINE_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        CLOSE_SOCK(s);
        return;
    }

    eng_sock_      = s;
    eng_connected_ = true;
    status_msg_    = "Connected to audio engine";

    /* Start receiver thread */
    eng_rx_thread_ = std::thread(&MainGUI::eng_receiver_loop, this);
}

void MainGUI::eng_disconnect() {
    if (eng_sock_ != BAD_SOCK) {
        CLOSE_SOCK(eng_sock_);
        eng_sock_ = BAD_SOCK;
    }
    if (eng_rx_thread_.joinable()) eng_rx_thread_.join();
    eng_connected_ = false;
    status_msg_    = "Disconnected";
}

void MainGUI::eng_reconnect_loop() {
    while (eng_running_) {
        if (!eng_connected_) {
            status_msg_ = "Reconnecting...";
            eng_connect();
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void MainGUI::eng_receiver_loop() {
    /* Reads interleaved float32 batches: [pre0, post0, pre1, post1, ...] */
    const int pair_bytes = PACKET_SAMPLES * 2 * (int)sizeof(float);
    std::vector<float> interleaved(PACKET_SAMPLES * 2);
    std::vector<float> pre_vec(PACKET_SAMPLES), post_vec(PACKET_SAMPLES);

    while (eng_running_ && eng_sock_ != BAD_SOCK) {
        /* Blocking recv of exactly one batch */
        int total = 0;
        char* raw = reinterpret_cast<char*>(interleaved.data());
        bool ok = true;
        while (total < pair_bytes) {
            int n = recv(eng_sock_, raw + total, pair_bytes - total, 0);
            if (n <= 0) { ok = false; break; }
            total += n;
        }
        if (!ok) break;

        if (!paused_) {
            /* De-interleave and push */
            for (int i = 0; i < PACKET_SAMPLES; ++i) {
                pre_vec[i]  = interleaved[i * 2];
                post_vec[i] = interleaved[i * 2 + 1];
            }
            push_batch(pre_vec.data(), post_vec.data(), PACKET_SAMPLES);
        }
    }

    eng_connected_ = false;
    status_msg_    = "Audio engine disconnected";
    if (eng_sock_ != BAD_SOCK) {
        CLOSE_SOCK(eng_sock_);
        eng_sock_ = BAD_SOCK;
    }
}

void MainGUI::eng_send_json(const std::string& json_str) {
    if (eng_sock_ == BAD_SOCK) return;
    std::string msg = json_str + "\n";
    send(eng_sock_, msg.c_str(), (int)msg.size(), 0);
}

void MainGUI::send_to_engine() {
    eng_send_json(PresetManager::to_audio_engine_json(current_preset_));
}

/* ── Push waveform batch (called from receiver thread) ───────────────────── */

void MainGUI::push_batch(const float* pre, const float* post, int n) {
    /* Convert raw samples to volts (matches Python's VREF=3.3 conversion) */
    constexpr float VREF = 3.3f;

    std::lock_guard<std::mutex> lk(buf_mtx_);
    for (int i = 0; i < n; ++i) {
        float pre_v  = (pre[i]  + 1.0f) * (VREF / 2.0f);
        float post_v = (post[i] + 1.0f) * (VREF / 2.0f);
        pre_buf_.push_back(pre_v);
        post_buf_.push_back(post_v);
    }
    while ((int)pre_buf_.size()  > BUFFER_MAX) pre_buf_.pop_front();
    while ((int)post_buf_.size() > BUFFER_MAX) post_buf_.pop_front();
}

/* ── Mobile app preset handler ───────────────────────────────────────────── */

void MainGUI::handle_app_preset(const json& data) {
    /* Called from AppReceiver background thread – only store, don't modify UI */
    std::lock_guard<std::mutex> lk(pending_preset_mtx_);
    pending_preset_data_ = data;
    has_pending_preset_  = true;
}

void MainGUI::apply_pending_preset() {
    /* Called from main render thread – safe to modify all UI state */
    json data;
    {
        std::lock_guard<std::mutex> lk(pending_preset_mtx_);
        if (!has_pending_preset_) return;
        data                = pending_preset_data_;
        has_pending_preset_ = false;
    }

    std::cout << "[gui] Applying preset received from mobile app\n";

    Preset incoming = PresetManager::from_json(data);
    std::string key = incoming.name;

    if ((int)preset_keys_.size() >= PresetManager::MAX_PRESETS &&
        std::find(preset_keys_.begin(), preset_keys_.end(), key) == preset_keys_.end())
    {
        /* Remove the oldest preset to make room */
        const std::string& oldest = preset_keys_.front();
        preset_mgr_.delete_preset(oldest);
        reload_preset_keys();
    }

    preset_mgr_.set_preset(key, incoming);
    preset_mgr_.save();
    reload_preset_keys();

    /* Activate the received preset */
    auto it = std::find(preset_keys_.begin(), preset_keys_.end(), key);
    if (it != preset_keys_.end())
        selected_preset_idx_ = (int)(it - preset_keys_.begin());
    current_preset_key_ = key;
    current_preset_     = incoming;
    effect_expanded_.assign(current_preset_.effects.size(), false);

    send_to_engine();
}
