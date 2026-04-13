#pragma once
/*
 * preset_model.hpp  –  C++ mirror of Interfaz/core/preset_model.py
 *
 * JSON format produced by to_json() is identical to the Python version so
 * that the audio engine (port 54321) and the Flutter mobile app receive
 * exactly the same payload regardless of which front-end is active.
 */

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/* One effect in the processing chain. */
struct Effect {
    std::string                      id;       // e.g. "fx_1"
    std::string                      type;     // e.g. "Overdrive"
    bool                             enabled{true};
    std::map<std::string, float>     params;
};

class PresetModel {
public:
    std::string         name;
    float               master_gain{1.0f};
    std::vector<Effect> effects;

    explicit PresetModel(const std::string& name = "Preset1");

    /* Mirrors Python PresetModel API ------------------------------------ */

    void set_effects(const std::vector<Effect>& effects_list);

    void update_param(const std::string& effect_id,
                      const std::string& param,
                      float              value);

    void update_order(const std::vector<Effect>& new_order);

    void toggle_effect(int effect_index, bool state);

    /* Parse a JSON object (already decoded) and populate this model.
     * Mirrors Python load_from_json(). */
    void load_from_json(const nlohmann::json& data);

    /* Serialise to a JSON string, identical format to Python to_json(). */
    std::string to_json() const;

    /* Default parameter map for each effect type. */
    static std::map<std::string, float> default_params(const std::string& type);

    /* All supported effect type names (same order as Python). */
    static const std::vector<std::string>& available_types();
};
