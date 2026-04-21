/*
 * preset_manager.cpp - Preset load/save implementation.
 */

#include "preset_manager.hpp"
#include <fstream>
#include <iostream>

/* ── Effect catalogue ────────────────────────────────────────────────────── */

const std::vector<std::string> PresetManager::AVAILABLE_EFFECTS = {
    "Overdrive", "Delay", "Wah", "Flanger",
    "Chorus", "Phaser", "PitchShifter", "Reverb"
};

/* ── Construction ────────────────────────────────────────────────────────── */

PresetManager::PresetManager(const std::string& file_path)
    : file_path_(file_path) {}

/* ── Disk I/O ────────────────────────────────────────────────────────────── */

bool PresetManager::load() {
    std::ifstream f(file_path_);
    if (!f.is_open()) {
        ensure_defaults();
        return save();
    }
    try {
        presets_data_ = json::parse(f).get<std::map<std::string, json>>();
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "[preset_manager] JSON parse error: " << ex.what() << "\n";
        ensure_defaults();
        return false;
    }
}

bool PresetManager::save() {
    std::ofstream f(file_path_);
    if (!f.is_open()) {
        std::cerr << "[preset_manager] Cannot write " << file_path_ << "\n";
        return false;
    }
    json out(presets_data_);
    f << out.dump(2);
    return true;
}

void PresetManager::ensure_defaults() {
    presets_data_ = {
        {"Preset 1", {{"name","Preset1"}, {"master_gain",1.0}, {"effects",json::array()}}},
        {"Preset 2", {{"name","Preset2"}, {"master_gain",1.0}, {"effects",json::array()}}},
        {"Preset 3", {{"name","Preset3"}, {"master_gain",1.0}, {"effects",json::array()}}},
    };
}

/* ── Preset access ───────────────────────────────────────────────────────── */

std::vector<std::string> PresetManager::preset_keys() const {
    std::vector<std::string> keys;
    keys.reserve(presets_data_.size());
    for (const auto& [k, v] : presets_data_)
        keys.push_back(k);
    return keys;
}

Preset PresetManager::get_preset(const std::string& key) const {
    auto it = presets_data_.find(key);
    if (it == presets_data_.end()) return {};
    return from_json(it->second);
}

void PresetManager::set_preset(const std::string& key, const Preset& preset) {
    json effects_arr = json::array();
    for (const auto& e : preset.effects) {
        json params_obj;
        for (const auto& [pn, pv] : e.params) params_obj[pn] = pv;
        effects_arr.push_back({
            {"id",      e.id},
            {"type",    e.type},
            {"enabled", e.enabled},
            {"params",  params_obj}
        });
    }
    presets_data_[key] = {
        {"name",        preset.name},
        {"master_gain", preset.master_gain},
        {"effects",     effects_arr}
    };
}

void PresetManager::delete_preset(const std::string& key) {
    presets_data_.erase(key);
}

void PresetManager::create_preset(const std::string& key, const std::string& name) {
    presets_data_[key] = {
        {"name",        name},
        {"master_gain", 1.0},
        {"effects",     json::array()}
    };
}

/* ── JSON (de)serialisation ──────────────────────────────────────────────── */

Preset PresetManager::from_json(const json& data) {
    Preset p;
    p.name        = data.value("name", "Preset");
    p.master_gain = data.value("master_gain", 1.0f);

    if (data.contains("effects") && data["effects"].is_array()) {
        for (size_t idx = 0; idx < data["effects"].size(); ++idx) {
            const auto& ej = data["effects"][idx];
            Effect ef;
            ef.id      = ej.value("id", "fx_" + std::to_string(idx + 1));
            ef.type    = ej.value("type", "Overdrive");
            ef.enabled = ej.value("enabled", true);
            if (ej.contains("params") && ej["params"].is_object()) {
                for (const auto& [pn, pv] : ej["params"].items())
                    ef.params[pn] = pv.get<float>();
            }
            p.effects.push_back(std::move(ef));
        }
    }
    return p;
}

std::string PresetManager::to_audio_engine_json(const Preset& preset) {
    json effects_arr = json::array();
    for (const auto& e : preset.effects) {
        json params_obj;
        for (const auto& [pn, pv] : e.params) params_obj[pn] = pv;
        effects_arr.push_back({
            {"id",      e.id},
            {"type",    e.type},
            {"enabled", e.enabled},
            {"params",  params_obj}
        });
    }
    json payload = {
        {"command",     "apply_preset"},
        {"name",        preset.name},
        {"master_gain", preset.master_gain},
        {"effects",     effects_arr}
    };
    return payload.dump(2);
}

/* ── Default parameters ──────────────────────────────────────────────────── */

std::map<std::string, float> PresetManager::default_params(const std::string& type) {
    static const std::map<std::string, std::map<std::string, float>> table = {
        {"Overdrive",   {{"GAIN",0.5f},{"TONE",0.5f},{"OUTPUT",0.5f}}},
        {"Delay",       {{"TIME",0.5f},{"FEEDBACK",0.3f},{"MIX",0.2f}}},
        {"Wah",         {{"FREQ",0.5f},{"Q",0.8f},{"LEVEL",1.0f}}},
        {"Flanger",     {{"RATE",0.5f},{"DEPTH",0.3f},{"FEEDBACK",0.2f},{"MIX",0.5f}}},
        {"Chorus",      {{"RATE",0.5f},{"DEPTH",0.3f},{"FEEDBACK",0.0f},{"MIX",0.2f}}},
        {"Phaser",      {{"RATE",0.5f},{"DEPTH",0.7f},{"FEEDBACK",0.3f},{"MIX",0.5f}}},
        {"PitchShifter",{{"SEMITONES",0.0f},{"SEMITONES_B",0.0f},
                         {"MIX_A",1.0f},{"MIX_B",0.0f},{"MIX",0.5f}}},
        {"Reverb",      {{"FEEDBACK",0.6f},{"LPFREQ",8000.0f},{"MIX",0.3f}}},
    };
    auto it = table.find(type);
    return (it != table.end()) ? it->second : std::map<std::string, float>{};
}

/* ── Parameter slider ranges ─────────────────────────────────────────────── */

ParamRange PresetManager::param_range(const std::string& param) {
    static const std::map<std::string, ParamRange> table = {
        {"GAIN",        {0.0f,   1.0f,     ""}},
        {"TONE",        {0.0f,   1.0f,     ""}},
        {"OUTPUT",      {0.0f,   1.0f,     ""}},
        {"TIME",        {1.0f,   1000.0f,  "ms"}},
        {"FEEDBACK",    {0.0f,   1.0f,     ""}},
        {"MIX",         {0.0f,   0.3f,     ""}},
        {"FREQ",        {300.0f, 4000.0f,  "Hz"}},
        {"Q",           {0.1f,   10.0f,    ""}},
        {"LEVEL",       {0.0f,   1.0f,     ""}},
        {"RATE",        {0.1f,   3.0f,     "Hz"}},
        {"DEPTH",       {0.0f,   1.0f,     ""}},
        {"SEMITONES",   {-12.0f, 12.0f,    "st"}},
        {"SEMITONES_B", {-12.0f, 12.0f,    "st"}},
        {"MIX_A",       {0.0f,   1.0f,     ""}},
        {"MIX_B",       {0.0f,   1.0f,     ""}},
        {"LPFREQ",      {2000.0f,10000.0f, "Hz"}},
    };
    auto it = table.find(param);
    return (it != table.end()) ? it->second : ParamRange{0.0f, 1.0f, ""};
}
