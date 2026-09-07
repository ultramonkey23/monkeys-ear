#pragma once

#include "monkeys_ear/types.h"
#include "monkeys_ear/sound_space.h"
#include "monkeys_ear/audio_input.h"
#include <string>
#include <array>
#include <cstring>
#include <ostream>

namespace monkeys_ear {

struct PresetName {
    std::array<char,64> value{};
    PresetName(const char* text="") { assign(text); }
    void assign(const char* text) { value.fill(0); if(text) std::strncpy(value.data(),text,value.size()-1); }
    PresetName& operator=(const char* text) { assign(text); return *this; }
    PresetName& operator=(const std::string& text) { assign(text.c_str()); return *this; }
    const char* c_str() const { return value.data(); }
    bool operator==(const PresetName& other) const { return value==other.value; }
    bool operator==(const char* other) const { return std::strcmp(value.data(),other)==0; }
};
inline std::ostream& operator<<(std::ostream& out,const PresetName& name){return out<<name.c_str();}

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
    PresetName name;
    std::array<float, NUM_MACROS> macros;
    float master_gain_db;
    int synth_waveform;
    float sub_mix;
    float fundamental_mix;
    int sub_ratio_denominator;
    float sub_phase;
    bool sub_polarity_inverted;
    float sub_saturation;
    float sub_envelope_amount;
    float source_level;
    float character_level;
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

    // Two-pass ZDF filter architecture
    int filter_mode_a;
    int filter_mode_b;
    float filter_cutoff_b_hz;
    float filter_resonance_b;
    float filter_drive;
    float filter_wet;
    int filter_routing;
    bool filter_slope_a_24db;
    bool filter_slope_b_24db;
    float filter_key_tracking;

    // Four-band production EQ (type, frequency, gain and Q per band)
    std::array<int, 4> eq_type;
    std::array<float, 4> eq_frequency_hz;
    std::array<float, 4> eq_gain_db;
    std::array<float, 4> eq_q;
    bool eq_bypass;
    bool eq_gain_compensation;

    // Bounded motion matrix: sources remain conventional unless state depths are used.
    int lfo1_waveform;
    float motion_curve;
    float mod_lfo_cutoff;
    float mod_lfo_resonance;
    float mod_lfo_fm;
    float mod_lfo_sub_blend;
    float mod_lfo_drive;
    float mod_lfo_eq_frequency;
    float mod_lfo_eq_gain;
    float mod_state_direction_filter;
    float mod_state_fast_fm;
    float mod_state_slow_balance;
    float mod_state_slow_resonator;
    float mod_audio_envelope_drive;

    // Performance architecture
    bool mono_mode;
    bool legato;
    float portamento_seconds;
    float pitch_bend_range;
    float vibrato_depth_semitones;
    float velocity_tone;
    float aftertouch_filter;
    float aftertouch_drive;

    // Bounded Sound Space: independent local pitch/partial/modal freedom plus
    // four generic source -> destination relationships. Appended for recall compatibility.
    SoundSpaceControls sound_space;

    // Appended vocal expression state preserves every legacy preset field.
    VocalExpressionControls vocal_expression;

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
    static PresetData create_monolith();
    static PresetData create_feral_wobble();
    static PresetData create_velvet_lead();
    static PresetData create_sound_space();

    PresetData& mutable_current() { return current_; }

private:
    PresetData current_;
};

} // namespace monkeys_ear
