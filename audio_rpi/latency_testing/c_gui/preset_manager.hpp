#pragma once
/*
 * preset_manager.hpp - Load/save presets from JSON file.
 *
 * JSON format is identical to the Python PresetModel so that the same
 * presets.json can be shared between the Python and C++ GUIs.
 *
 * Preset file layout (presets.json):
 * {
 *   "Preset 1": {
 *     "name": "Preset1",
 *     "master_gain": 1.0,
 *     "effects": [
 *       { "id": "fx_1", "type": "Overdrive", "enabled": true,
 *         "params": { "GAIN": 0.5, "TONE": 0.3, "OUTPUT": 0.8 } }
 *     ]
 *   }
 * }
 *
 * Command sent to audio engine (port 54321):
 * {
 *   "command": "apply_preset",
 *   "name": "Preset1",
 *   "master_gain": 1.0,
 *   "effects": [ ... ]
 * }
 */

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Parameter display range (mirrors Python effect_widget.py PARAM_RANGES) */
struct ParamRange {
    float       min_val;
    float       max_val;
    std::string unit;
};

/* One effect in the active chain */
struct Effect {
    std::string                  id;
    std::string                  type;
    bool                         enabled = true;
    std::map<std::string, float> params;
};

/* One complete preset */
struct Preset {
    std::string          name        = "Preset";
    float                master_gain = 1.0f;
    std::vector<Effect>  effects;
};

class PresetManager {
public:
    /* All eight effect types the Python GUI supports */
    static const std::vector<std::string> AVAILABLE_EFFECTS;
    static constexpr int MAX_EFFECTS  = 4;
    static constexpr int MAX_PRESETS  = 5;   /* max presets before oldest is evicted */

    explicit PresetManager(const std::string& file_path = "presets.json");

    /* Load presets from disk.  Creates default presets if file is missing. */
    bool load();

    /* Persist current state to disk. */
    bool save();

    /* Ordered list of preset keys as stored in the JSON. */
    std::vector<std::string> preset_keys() const;

    /* Retrieve a preset by key. */
    Preset get_preset(const std::string& key) const;

    /* Overwrite (or create) a preset entry. */
    void set_preset(const std::string& key, const Preset& preset);

    /* Remove a preset. */
    void delete_preset(const std::string& key);

    /* Create an empty preset with the given name. */
    void create_preset(const std::string& key, const std::string& name);

    /* Serialise a preset to the JSON string expected by the audio engine. */
    static std::string to_audio_engine_json(const Preset& preset);

    /* Deserialise a Preset from a json object (used by app receiver). */
    static Preset from_json(const json& data);

    /* Default parameter values for a given effect type. */
    static std::map<std::string, float> default_params(const std::string& effect_type);

    /* Slider display range for a parameter name. */
    static ParamRange param_range(const std::string& param_name);

private:
    std::string          file_path_;
    std::map<std::string, json> presets_data_;  /* sorted by key (alphabetical) */

    void ensure_defaults();
};
