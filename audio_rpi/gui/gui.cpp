/*
 * gui.cpp  –  ImGui rendering for the MultiFX Processor GUI.
 *
 * Layout (top to bottom):
 *   ┌─ Preset bar ────────────────────────────────────────────────────────┐
 *   │  [Preset 1 ▼]  [Save]  [Load]                                       │
 *   ├─ Master Gain ───────────────────────────────────────────────────────┤
 *   │  [=======slider========]  1.00 (0.00 dB)                            │
 *   ├─ Effect Chain ──────────────────────────────────────────────────────┤
 *   │  [Add Type ▼]  [Add]   [Remove]  [▲ Up]  [▼ Down]  [Toggle]        │
 *   │  ┌─ fx_1: Overdrive  [ON] ───────────────────────────────────────┐  │
 *   │  │  GAIN [slider]  TONE [slider]  OUTPUT [slider]                │  │
 *   │  └────────────────────────────────────────────────────────────────┘  │
 *   ├─ Waveform ──────────────────────────────────────────────────────────┤
 *   │  Pre:  [plot]                                                        │
 *   │  Post: [plot]                                                        │
 *   └─ Status bar ────────────────────────────────────────────────────────┘
 */

#include "gui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <imgui.h>

static const char* PRESETS_FILE = "presets.json";

/* ── Construction / destruction ───────────────────────────────────────────── */

AudioGui::AudioGui() {
    /* Default three empty presets (matches Python _load_presets_file) */
    for (const char* k : {"Preset 1", "Preset 2", "Preset 3"}) {
        presets_.emplace(k, PresetModel(k));
        preset_keys_.push_back(k);
    }
    current_key_ = preset_keys_[0];
}

AudioGui::~AudioGui() {
    client_.disconnect();
    app_rx_.stop();
}

/* ── init() ───────────────────────────────────────────────────────────────── */

void AudioGui::init(const std::string& host, int port) {
    host_ = host;
    port_ = port;

    load_presets_file();
    if (!preset_keys_.empty()) {
        preset_combo_idx_ = 0;
        current_key_      = preset_keys_[0];
    }

    /* Start mobile-app receiver on port 5000 */
    app_rx_.start([this](const nlohmann::json& j) { on_remote_json(j); });

    try_connect();
}

/* ── Per-frame render ─────────────────────────────────────────────────────── */

void AudioGui::render(float dt) {
    /* Reconnect timer */
    if (!connected_) {
        reconnect_timer_ += dt;
        if (reconnect_timer_ >= 2.0f) {
            reconnect_timer_ = 0.0f;
            try_connect();
        }
    }

    snap_waveform();

    /* Full-viewport window */
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar     |
                 ImGuiWindowFlags_NoResize        |
                 ImGuiWindowFlags_NoMove          |
                 ImGuiWindowFlags_NoScrollbar     |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    /* Title */
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("  MultiFX Processor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();

    render_preset_bar();
    ImGui::Separator();
    render_gain_slider();
    ImGui::Separator();
    render_effect_chain();
    ImGui::Separator();
    render_waveforms();
    ImGui::Separator();
    render_status_bar();

    ImGui::End();
}

/* ── Preset bar ───────────────────────────────────────────────────────────── */

void AudioGui::render_preset_bar() {
    ImGui::Text("Preset:");
    ImGui::SameLine();

    /* Build combo items string */
    std::string items;
    for (const auto& k : preset_keys_) { items += k; items += '\0'; }
    items += '\0';

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::Combo("##preset", &preset_combo_idx_, items.c_str())) {
        current_key_ = preset_keys_[static_cast<size_t>(preset_combo_idx_)];
        send_preset();
    }

    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        save_presets_file();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load file")) {
        load_presets_file();
        if (!preset_keys_.empty()) {
            preset_combo_idx_ = 0;
            current_key_      = preset_keys_[0];
            send_preset();
        }
    }
}

/* ── Master gain ──────────────────────────────────────────────────────────── */

void AudioGui::render_gain_slider() {
    PresetModel& pm = current_preset();

    ImGui::Text("Master Gain");
    ImGui::SameLine();

    float gain = pm.master_gain;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160.0f);
    if (ImGui::SliderFloat("##gain", &gain, 0.0f, 4.0f, "%.2f")) {
        pm.master_gain = gain;
        send_preset();
    }

    ImGui::SameLine();
    /* Show dB value */
    float db = (gain > 0.0001f) ? (20.0f * std::log10(gain)) : -60.0f;
    ImGui::Text("%.2f dB", db);
}

/* ── Effect chain ─────────────────────────────────────────────────────────── */

void AudioGui::render_effect_chain() {
    PresetModel& pm = current_preset();

    ImGui::Text("Effect Chain");

    /* Add-effect controls */
    const auto& types = PresetModel::available_types();
    std::string type_items;
    for (const auto& t : types) { type_items += t; type_items += '\0'; }
    type_items += '\0';

    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("##addtype", &add_type_idx_, type_items.c_str());
    ImGui::SameLine();

    if (ImGui::Button("Add")) {
        if (pm.effects.size() < 8) {
            const std::string& t = types[static_cast<size_t>(add_type_idx_)];
            bool already = false;
            for (const auto& fx : pm.effects)
                if (fx.type == t) { already = true; break; }
            if (!already) {
                Effect e;
                e.id      = "fx_" + std::to_string(pm.effects.size() + 1);
                e.type    = t;
                e.enabled = true;
                e.params  = PresetModel::default_params(t);
                pm.effects.push_back(std::move(e));
                send_preset();
                save_presets_file();
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove") && selected_fx_ >= 0 &&
        selected_fx_ < static_cast<int>(pm.effects.size())) {
        pm.effects.erase(pm.effects.begin() + selected_fx_);
        selected_fx_ = -1;
        send_preset();
        save_presets_file();
    }

    ImGui::SameLine();
    if (ImGui::Button("Up") && selected_fx_ > 0) {
        std::swap(pm.effects[static_cast<size_t>(selected_fx_)],
                  pm.effects[static_cast<size_t>(selected_fx_ - 1)]);
        selected_fx_--;
        send_preset();
        save_presets_file();
    }

    ImGui::SameLine();
    if (ImGui::Button("Down") &&
        selected_fx_ >= 0 &&
        selected_fx_ < static_cast<int>(pm.effects.size()) - 1) {
        std::swap(pm.effects[static_cast<size_t>(selected_fx_)],
                  pm.effects[static_cast<size_t>(selected_fx_ + 1)]);
        selected_fx_++;
        send_preset();
        save_presets_file();
    }

    ImGui::SameLine();
    if (ImGui::Button("Toggle") &&
        selected_fx_ >= 0 &&
        selected_fx_ < static_cast<int>(pm.effects.size())) {
        auto& fx = pm.effects[static_cast<size_t>(selected_fx_)];
        fx.enabled = !fx.enabled;
        send_preset();
        save_presets_file();
    }

    /* List of effects with per-effect parameter sliders */
    ImGui::BeginChild("##fx_list",
                      {0, 200},
                      true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < static_cast<int>(pm.effects.size()); i++) {
        Effect& fx = pm.effects[static_cast<size_t>(i)];

        /* Header row – selectable */
        bool is_sel = (selected_fx_ == i);
        ImGui::PushID(i);

        const char* on_off = fx.enabled ? "[ON] " : "[OFF]";
        ImVec4 col = fx.enabled
            ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f)
            : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);

        char label[64];
        snprintf(label, sizeof(label), "%s %s: %s",
                 on_off, fx.id.c_str(), fx.type.c_str());

        if (ImGui::Selectable(label, is_sel,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            selected_fx_ = i;
            if (ImGui::IsMouseDoubleClicked(0)) {
                fx.enabled = !fx.enabled;
                send_preset();
                save_presets_file();
            }
        }
        ImGui::PopStyleColor();

        /* Parameter sliders (shown for all effects, indented) */
        ImGui::Indent(16.0f);
        bool changed = false;
        for (auto& [pname, pval] : fx.params) {
            /* Determine slider range */
            float lo = 0.0f, hi = 1.0f;
            if (pname == "SEMITONES" || pname == "SEMITONES_B") { lo = -24.0f; hi = 24.0f; }
            else if (pname == "LPFREQ") { lo = 100.0f; hi = 20000.0f; }
            else if (pname == "TIME")   { lo = 0.0f;   hi = 2.0f; }

            char slider_id[32];
            snprintf(slider_id, sizeof(slider_id), "##%s", pname.c_str());

            ImGui::Text("  %-12s", pname.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat(slider_id, &pval, lo, hi, "%.3f"))
                changed = true;
        }
        ImGui::Unindent(16.0f);

        if (changed) {
            send_preset();
            save_presets_file();
        }

        ImGui::PopID();
        ImGui::Separator();
    }

    ImGui::EndChild();
}

/* ── Waveform display ─────────────────────────────────────────────────────── */

void AudioGui::render_waveforms() {
    ImGui::Text("Waveform");

    float avail_w = ImGui::GetContentRegionAvail().x;
    float plot_h  = 80.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.78f, 1.0f, 1.0f));
    ImGui::PlotLines("##pre",
                     disp_pre_.data(),
                     WAVEFORM_DISPLAY,
                     0,
                     "Pre",
                     -1.5f, 1.5f,
                     {avail_w, plot_h});
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
    ImGui::PlotLines("##post",
                     disp_post_.data(),
                     WAVEFORM_DISPLAY,
                     0,
                     "Post",
                     -1.5f, 1.5f,
                     {avail_w, plot_h});
    ImGui::PopStyleColor(2);
}

/* ── Status bar ───────────────────────────────────────────────────────────── */

void AudioGui::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_Text,
                          connected_
                          ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f)
                          : ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
    ImGui::Text("  %s", status_buf_);
    ImGui::PopStyleColor();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f +
                    ImGui::GetCursorPosX());
    if (ImGui::SmallButton("Quit"))
        want_close_ = true;
}

/* ── Waveform ring-buffer helpers ─────────────────────────────────────────── */

void AudioGui::on_batch(const float* pre, const float* post, int n) {
    std::lock_guard<std::mutex> lk(ring_mutex_);
    int w = ring_write_.load();
    for (int i = 0; i < n; i++) {
        int idx          = (w + i) % WAVEFORM_RING;
        pre_ring_[idx]   = pre[i];
        post_ring_[idx]  = post[i];
    }
    ring_write_.store((w + n) % WAVEFORM_RING);
}

void AudioGui::snap_waveform() {
    std::lock_guard<std::mutex> lk(ring_mutex_);
    int w = ring_write_.load();
    for (int i = 0; i < WAVEFORM_DISPLAY; i++) {
        int idx         = (w - WAVEFORM_DISPLAY + i + WAVEFORM_RING) % WAVEFORM_RING;
        disp_pre_[i]    = pre_ring_[idx];
        disp_post_[i]   = post_ring_[idx];
    }
}

/* ── Remote JSON (mobile app) ─────────────────────────────────────────────── */

void AudioGui::on_remote_json(const nlohmann::json& data) {
    PresetModel& pm = current_preset();
    pm.load_from_json(data);
    /* Forward to audio engine */
    client_.send_json(pm.to_json());
    save_presets_file();
}

/* ── Connection helpers ───────────────────────────────────────────────────── */

void AudioGui::try_connect() {
    if (connected_) return;
    if (client_.connect(host_, port_)) {
        connected_ = true;
        snprintf(status_buf_, sizeof(status_buf_),
                 "Connected to audio engine on port %d", port_);
        client_.start([this](const float* pre, const float* post, int n) {
            on_batch(pre, post, n);
        });
        /* Push current preset to engine on connect */
        send_preset();
    } else {
        snprintf(status_buf_, sizeof(status_buf_),
                 "Connecting to %s:%d ...", host_.c_str(), port_);
    }
}

void AudioGui::send_preset() {
    if (!connected_) return;
    std::string json = current_preset().to_json();
    if (!client_.send_json(json)) {
        connected_ = false;
        snprintf(status_buf_, sizeof(status_buf_), "Disconnected – reconnecting...");
    }
}

/* ── Preset file I/O ──────────────────────────────────────────────────────── */

void AudioGui::load_presets_file() {
    std::ifstream f(PRESETS_FILE);
    if (!f.is_open()) {
        /* Create default file */
        save_presets_file();
        return;
    }

    try {
        nlohmann::json root;
        f >> root;

        presets_.clear();
        preset_keys_.clear();

        for (auto& [key, val] : root.items()) {
            PresetModel pm(key);
            pm.load_from_json(val);
            presets_.emplace(key, std::move(pm));
            preset_keys_.push_back(key);
        }
    } catch (...) {
        fprintf(stderr, "[gui] Failed to parse %s\n", PRESETS_FILE);
    }

    if (preset_keys_.empty()) {
        for (const char* k : {"Preset 1", "Preset 2", "Preset 3"}) {
            presets_.emplace(k, PresetModel(k));
            preset_keys_.push_back(k);
        }
    }
    current_key_ = preset_keys_[0];
}

void AudioGui::save_presets_file() {
    nlohmann::json root;
    for (const auto& key : preset_keys_) {
        const PresetModel& pm = presets_.at(key);
        nlohmann::json pm_json;
        pm_json["name"]        = pm.name;
        pm_json["master_gain"] = pm.master_gain;

        nlohmann::json fx_arr = nlohmann::json::array();
        for (const auto& fx : pm.effects) {
            nlohmann::json fx_obj;
            fx_obj["id"]      = fx.id;
            fx_obj["type"]    = fx.type;
            fx_obj["enabled"] = fx.enabled;
            nlohmann::json params = nlohmann::json::object();
            for (const auto& [k, v] : fx.params) params[k] = v;
            fx_obj["params"] = params;
            fx_arr.push_back(fx_obj);
        }
        pm_json["effects"] = fx_arr;
        root[key] = pm_json;
    }

    std::ofstream f(PRESETS_FILE);
    if (f.is_open())
        f << root.dump(2) << '\n';
}

/* ── Accessors ────────────────────────────────────────────────────────────── */

PresetModel& AudioGui::current_preset() {
    return presets_.at(current_key_);
}
