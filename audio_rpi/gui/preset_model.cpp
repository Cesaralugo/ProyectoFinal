/*
 * preset_model.cpp  –  C++ mirror of Interfaz/core/preset_model.py
 */

#include "preset_model.hpp"

#include <sstream>
#include <stdexcept>

/* ── Construction ─────────────────────────────────────────────────────────── */

PresetModel::PresetModel(const std::string& name_)
    : name(name_), master_gain(1.0f) {}

/* ── Chain manipulation ───────────────────────────────────────────────────── */

void PresetModel::set_effects(const std::vector<Effect>& effects_list) {
    effects = effects_list;
}

void PresetModel::update_param(const std::string& effect_id,
                               const std::string& param,
                               float              value) {
    for (auto& fx : effects) {
        if (fx.id == effect_id) {
            fx.params[param] = value;
            break;
        }
    }
}

void PresetModel::update_order(const std::vector<Effect>& new_order) {
    effects = new_order;
}

void PresetModel::toggle_effect(int effect_index, bool state) {
    if (effect_index >= 0 && effect_index < static_cast<int>(effects.size()))
        effects[static_cast<size_t>(effect_index)].enabled = state;
}

/* ── JSON (mirrors Python exactly) ────────────────────────────────────────── */

void PresetModel::load_from_json(const nlohmann::json& data) {
    if (data.contains("name") && data["name"].is_string())
        name = data["name"].get<std::string>();

    if (data.contains("master_gain"))
        master_gain = data["master_gain"].get<float>();

    effects.clear();

    if (!data.contains("effects") || !data["effects"].is_array())
        return;

    int index = 0;
    for (const auto& fx : data["effects"]) {
        Effect e;

        /* FIX: if Flutter doesn't send id (or sends null) generate one */
        if (fx.contains("id") && !fx["id"].is_null() &&
            fx["id"].is_string() && !fx["id"].get<std::string>().empty()) {
            e.id = fx["id"].get<std::string>();
        } else {
            e.id = "fx_" + std::to_string(index + 1);
        }

        e.type    = fx.value("type",    "");
        e.enabled = fx.value("enabled", true);

        if (fx.contains("params") && fx["params"].is_object()) {
            for (auto& [k, v] : fx["params"].items()) {
                if (v.is_number())
                    e.params[k] = v.get<float>();
            }
        }

        effects.push_back(std::move(e));
        ++index;
    }
}

std::string PresetModel::to_json() const {
    nlohmann::json payload;
    payload["command"]     = "apply_preset";
    payload["name"]        = name;
    payload["master_gain"] = master_gain;

    nlohmann::json effects_arr = nlohmann::json::array();
    for (const auto& fx : effects) {
        nlohmann::json fx_obj;
        fx_obj["id"]      = fx.id;
        fx_obj["type"]    = fx.type;
        fx_obj["enabled"] = fx.enabled;

        nlohmann::json params_obj = nlohmann::json::object();
        for (const auto& [k, v] : fx.params)
            params_obj[k] = v;
        fx_obj["params"] = params_obj;

        effects_arr.push_back(fx_obj);
    }
    payload["effects"] = effects_arr;

    return payload.dump(2);
}

/* ── Default parameters (mirrors Python default_params()) ─────────────────── */

std::map<std::string, float> PresetModel::default_params(const std::string& type) {
    static const std::map<std::string, std::map<std::string, float>> defaults = {
        {"Overdrive",    {{"GAIN", 0.5f}, {"TONE", 0.5f}, {"OUTPUT", 0.5f}}},
        {"Delay",        {{"TIME", 0.5f}, {"FEEDBACK", 0.3f}, {"MIX", 0.2f}}},
        {"Wah",          {{"FREQ", 0.5f}, {"Q", 0.8f}, {"LEVEL", 1.0f}}},
        {"Flanger",      {{"RATE", 0.5f}, {"DEPTH", 0.3f}, {"FEEDBACK", 0.2f}, {"MIX", 0.5f}}},
        {"Chorus",       {{"RATE", 0.5f}, {"DEPTH", 0.3f}, {"FEEDBACK", 0.0f}, {"MIX", 0.2f}}},
        {"Phaser",       {{"RATE", 0.5f}, {"DEPTH", 0.7f}, {"FEEDBACK", 0.3f}, {"MIX", 0.5f}}},
        {"PitchShifter", {{"SEMITONES", 0.0f}, {"SEMITONES_B", 0.0f},
                          {"MIX_A", 1.0f}, {"MIX_B", 0.0f}, {"MIX", 0.5f}}},
        {"Reverb",       {{"FEEDBACK", 0.6f}, {"LPFREQ", 8000.0f}, {"MIX", 0.3f}}},
    };

    auto it = defaults.find(type);
    if (it != defaults.end())
        return it->second;
    return {};
}

const std::vector<std::string>& PresetModel::available_types() {
    static const std::vector<std::string> types = {
        "Overdrive", "Delay", "Wah", "Flanger",
        "Chorus", "Phaser", "PitchShifter", "Reverb"
    };
    return types;
}
