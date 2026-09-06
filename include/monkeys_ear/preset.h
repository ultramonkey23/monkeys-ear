#pragma once

#include "monkeys_ear/types.h"
#include <string>
#include <array>

namespace monkeys_ear {

enum MacroId {
    MACRO_CUTOFF = 0,     // Macro 1: Brightness / Filter Cutoff
    MACRO_RESONANCE = 1,  // Macro 2: Bite / Filter Resonance
    MACRO_DRIVE = 2,      // Macro 3: Heat / Tube Saturation
    MACRO_BODY = 3,       // Macro 4: Body / Cabinet Resonance
    MACRO_DELAY = 4,      // Macro 5: Echo / Stereo Delay Mix & Time
    MACRO_SPACE = 5,      // Macro 6: Space / FDN Reverb Depth & Decay
    MACRO_MIC_BLEND = 6,  // Macro 7: Mic Blend / Live Input Mix
    MACRO_CHARACTER = 7,  // Macro 8: Character / Dynamic Memory Sag
    NUM_MACROS = 8
};

struct PresetData {
    std::string name;
    std::array<float, NUM_MACROS> macros;
    float master_gain_db;
    int synth_waveform;
    float sub_mix;
    float noise_mix;
    float amp_attack;
    float amp_decay;
    float amp_sustain;
    float amp_release;
    float filter_attack;
    float filter_decay;
    float filter_sustain;
    float filter_release;
    float filter_env_amount;

    PresetData();
    std::string serialize() const;
    bool deserialize(const std::string& data);
};

class PresetManager {
public:
    PresetManager();
    const PresetData& get_current() const { return current_; }
    void set_macro(MacroId id, float val);
    float get_macro(MacroId id) const;

    void load_init_preset();
    void load_preset(const PresetData& preset);
    void set_preset_name(const std::string& name);

    static PresetData create_factory_lead();
    static PresetData create_factory_bass();
    static PresetData create_factory_vocal_resonator();

private:
    PresetData current_;
};

} // namespace monkeys_ear
