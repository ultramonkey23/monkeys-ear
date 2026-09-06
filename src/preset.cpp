#include "monkeys_ear/preset.h"
#include <sstream>
#include <iomanip>

namespace monkeys_ear {

PresetData::PresetData()
    : name("Init Patch"),
      master_gain_db(0.0f),
      synth_waveform(0),
      sub_mix(0.20f),
      noise_mix(0.02f),
      amp_attack(0.005f),
      amp_decay(0.150f),
      amp_sustain(0.70f),
      amp_release(0.250f),
      filter_attack(0.010f),
      filter_decay(0.200f),
      filter_sustain(0.30f),
      filter_release(0.300f),
      filter_env_amount(0.50f) {
    macros[MACRO_CUTOFF] = 0.65f;     // Open bright cutoff
    macros[MACRO_RESONANCE] = 0.25f;  // Moderate bite
    macros[MACRO_DRIVE] = 0.30f;      // Warm tube drive
    macros[MACRO_BODY] = 0.40f;       // Acoustic chassis body
    macros[MACRO_DELAY] = 0.25f;      // Subtle stereo delay
    macros[MACRO_SPACE] = 0.35f;      // FDN Reverb room
    macros[MACRO_MIC_BLEND] = 0.0f;   // Synth primary (mic 0.0)
    macros[MACRO_CHARACTER] = 0.50f;  // Dynamic cathode sag
}

std::string PresetData::serialize() const {
    std::ostringstream oss;
    oss << "NAME=" << name << "\n";
    for (size_t i = 0; i < NUM_MACROS; ++i) {
        oss << "MACRO_" << i << "=" << macros[i] << "\n";
    }
    oss << "MASTER_GAIN=" << master_gain_db << "\n";
    oss << "WAVEFORM=" << synth_waveform << "\n";
    oss << "SUB_MIX=" << sub_mix << "\n";
    oss << "NOISE_MIX=" << noise_mix << "\n";
    oss << "AMP_A=" << amp_attack << "\n";
    oss << "AMP_D=" << amp_decay << "\n";
    oss << "AMP_S=" << amp_sustain << "\n";
    oss << "AMP_R=" << amp_release << "\n";
    oss << "FLT_A=" << filter_attack << "\n";
    oss << "FLT_D=" << filter_decay << "\n";
    oss << "FLT_S=" << filter_sustain << "\n";
    oss << "FLT_R=" << filter_release << "\n";
    oss << "FLT_ENV=" << filter_env_amount << "\n";
    return oss.str();
}

bool PresetData::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val_str = line.substr(eq + 1);

        if (key == "NAME") {
            name = val_str;
        } else if (key.rfind("MACRO_", 0) == 0) {
            int idx = std::stoi(key.substr(6));
            if (idx >= 0 && idx < NUM_MACROS) {
                macros[idx] = std::stof(val_str);
            }
        } else if (key == "MASTER_GAIN") master_gain_db = std::stof(val_str);
        else if (key == "WAVEFORM") synth_waveform = std::stoi(val_str);
        else if (key == "SUB_MIX") sub_mix = std::stof(val_str);
        else if (key == "NOISE_MIX") noise_mix = std::stof(val_str);
        else if (key == "AMP_A") amp_attack = std::stof(val_str);
        else if (key == "AMP_D") amp_decay = std::stof(val_str);
        else if (key == "AMP_S") amp_sustain = std::stof(val_str);
        else if (key == "AMP_R") amp_release = std::stof(val_str);
        else if (key == "FLT_A") filter_attack = std::stof(val_str);
        else if (key == "FLT_D") filter_decay = std::stof(val_str);
        else if (key == "FLT_S") filter_sustain = std::stof(val_str);
        else if (key == "FLT_R") filter_release = std::stof(val_str);
        else if (key == "FLT_ENV") filter_env_amount = std::stof(val_str);
    }
    return true;
}

PresetManager::PresetManager() {
    load_init_preset();
}

void PresetManager::set_macro(MacroId id, float val) {
    if (id >= 0 && id < NUM_MACROS) {
        current_.macros[id] = clamp(val, 0.0f, 1.0f);
    }
}

float PresetManager::get_macro(MacroId id) const {
    if (id >= 0 && id < NUM_MACROS) {
        return current_.macros[id];
    }
    return 0.0f;
}

void PresetManager::load_init_preset() {
    current_ = PresetData();
}

void PresetManager::load_preset(const PresetData& preset) {
    current_ = preset;
}

void PresetManager::set_preset_name(const std::string& name) {
    current_.name = name;
}

PresetData PresetManager::create_factory_lead() {
    PresetData p;
    p.name = "Savage Monolith Lead";
    p.macros[MACRO_CUTOFF] = 0.70f;
    p.macros[MACRO_RESONANCE] = 0.45f;
    p.macros[MACRO_DRIVE] = 0.55f;
    p.macros[MACRO_BODY] = 0.50f;
    p.macros[MACRO_DELAY] = 0.35f;
    p.macros[MACRO_SPACE] = 0.40f;
    p.macros[MACRO_MIC_BLEND] = 0.0f;
    p.macros[MACRO_CHARACTER] = 0.60f;
    p.synth_waveform = 0; // Saw
    p.amp_attack = 0.008f;
    p.amp_decay = 0.200f;
    p.amp_sustain = 0.85f;
    p.amp_release = 0.300f;
    p.filter_env_amount = 0.65f;
    return p;
}

PresetData PresetManager::create_factory_bass() {
    PresetData p;
    p.name = "Sub Harmonic Beast";
    p.macros[MACRO_CUTOFF] = 0.35f;
    p.macros[MACRO_RESONANCE] = 0.30f;
    p.macros[MACRO_DRIVE] = 0.65f;
    p.macros[MACRO_BODY] = 0.75f;
    p.macros[MACRO_DELAY] = 0.0f;
    p.macros[MACRO_SPACE] = 0.15f;
    p.macros[MACRO_MIC_BLEND] = 0.0f;
    p.macros[MACRO_CHARACTER] = 0.70f;
    p.synth_waveform = 1; // Pulse
    p.sub_mix = 0.60f;
    p.amp_attack = 0.002f;
    p.amp_decay = 0.350f;
    p.amp_sustain = 0.50f;
    p.amp_release = 0.150f;
    p.filter_env_amount = 0.40f;
    return p;
}

PresetData PresetManager::create_factory_vocal_resonator() {
    PresetData p;
    p.name = "Live Vocal Resonator & Echo";
    p.macros[MACRO_CUTOFF] = 0.80f;
    p.macros[MACRO_RESONANCE] = 0.40f;
    p.macros[MACRO_DRIVE] = 0.40f;
    p.macros[MACRO_BODY] = 0.85f;
    p.macros[MACRO_DELAY] = 0.45f;
    p.macros[MACRO_SPACE] = 0.60f;
    p.macros[MACRO_MIC_BLEND] = 1.0f; // 100% live microphone
    p.macros[MACRO_CHARACTER] = 0.50f;
    return p;
}

} // namespace monkeys_ear
