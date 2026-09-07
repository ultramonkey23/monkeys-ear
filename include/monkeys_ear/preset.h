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

    // Advanced Sound Construction & Cody's Mathematical State
    float osc_fm_amount;       // 0.0 to 1.0 (Phase Modulation cross-depth)
    int osc2_semi;             // -24 to +24 semitones
    bool osc_hard_sync;        // Sync Osc1 to Osc2
    float state_resistance;    // 0.0 to 1.0 (rho: history & strain opposing force)
    float state_repulsion;     // 0.0 to 1.0 (k_repel: negative-gravity soft-core repulsion)
    float state_coupling;      // 0.0 to 1.0 (signed phase-resonance coupling)
    float state_persistence;   // 0.0 to 1.0 (multiscale macro memory ratio)
    bool state_enabled;        // true = stateful body active; false = conventional baseline
    float lfo1_rate_hz;        // 0.05 to 30.0 Hz
    float lfo1_depth;          // 0.0 to 1.0
    int input_route_mode;      // 0 = Synth only, 1 = Blend, 2 = External audio direct

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
    static PresetData create_factory_stateful_bell();
    static PresetData create_factory_vocal_resonator();
    static PresetData create_factory_guitar_processor();

private:
    PresetData current_;
};

} // namespace monkeys_ear
