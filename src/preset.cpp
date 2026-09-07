#include "monkeys_ear/preset.h"
#include <sstream>
#include <iomanip>

namespace monkeys_ear {

PresetData::PresetData()
    : name("Init Patch"),
      master_gain_db(0.0f),
      synth_waveform(0),
      sub_mix(0.20f),
      fundamental_mix(0.20f),
      sub_ratio_denominator(2),
      sub_phase(0.0f),
      sub_polarity_inverted(false),
      sub_saturation(0.0f),
      sub_envelope_amount(1.0f),
      source_level(0.85f),
      character_level(0.75f),
      noise_mix(0.02f),
      amp_attack(0.005f),
      amp_decay(0.150f),
      amp_sustain(0.70f),
      amp_release(0.250f),
      filter_attack(0.010f),
      filter_decay(0.200f),
      filter_sustain(0.30f),
      filter_release(0.300f),
      filter_env_amount(0.50f),
      osc_fm_amount(0.0f),
      osc2_semi(0),
      osc_hard_sync(false),
      state_resistance(0.35f),
      state_repulsion(0.40f),
      state_coupling(0.30f),
      state_persistence(0.50f),
      state_enabled(true),
      lfo1_rate_hz(1.5f),
      lfo1_depth(0.0f),
      input_route_mode(0),
      filter_mode_a(0), filter_mode_b(0), filter_cutoff_b_hz(12000.0f),
      filter_resonance_b(0.05f), filter_drive(0.0f), filter_wet(1.0f),
      filter_routing(0), filter_slope_a_24db(false), filter_slope_b_24db(false),
      filter_key_tracking(0.0f),
      eq_type{{0,0,0,0}}, eq_frequency_hz{{55.0f,250.0f,1800.0f,9000.0f}},
      eq_gain_db{{0,0,0,0}}, eq_q{{0.707f,0.8f,1.0f,0.707f}},
      eq_bypass(false), eq_gain_compensation(true),
      lfo1_waveform(0), motion_curve(0.35f), mod_lfo_cutoff(0.0f),
      mod_lfo_resonance(0.0f), mod_lfo_fm(0.0f), mod_lfo_sub_blend(0.0f),
      mod_lfo_drive(0.0f), mod_lfo_eq_frequency(0.0f), mod_lfo_eq_gain(0.0f),
      mod_state_direction_filter(0.0f), mod_state_fast_fm(0.0f),
      mod_state_slow_balance(0.0f), mod_state_slow_resonator(0.0f),
      mod_audio_envelope_drive(0.0f), mono_mode(false), legato(false),
      portamento_seconds(0.0f), pitch_bend_range(2.0f), vibrato_depth_semitones(0.0f),
      velocity_tone(0.0f), aftertouch_filter(0.0f), aftertouch_drive(0.0f) {
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
    oss << "FUND_MIX=" << fundamental_mix << "\nSUB_RATIO=" << sub_ratio_denominator << "\n";
    oss << "SUB_PHASE=" << sub_phase << "\nSUB_POLARITY=" << (sub_polarity_inverted?1:0) << "\n";
    oss << "SUB_SAT=" << sub_saturation << "\nSUB_ENV=" << sub_envelope_amount << "\n";
    oss << "SOURCE_LEVEL=" << source_level << "\nCHAR_LEVEL=" << character_level << "\n";
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
    oss << "OSC_FM=" << osc_fm_amount << "\n";
    oss << "OSC2_SEMI=" << osc2_semi << "\n";
    oss << "OSC_SYNC=" << (osc_hard_sync ? 1 : 0) << "\n";
    oss << "STATE_RESIST=" << state_resistance << "\n";
    oss << "STATE_REPEL=" << state_repulsion << "\n";
    oss << "STATE_COUPLE=" << state_coupling << "\n";
    oss << "STATE_PERSIST=" << state_persistence << "\n";
    oss << "STATE_EN=" << (state_enabled ? 1 : 0) << "\n";
    oss << "LFO1_RATE=" << lfo1_rate_hz << "\n";
    oss << "LFO1_DEPTH=" << lfo1_depth << "\n";
    oss << "ROUTE_MODE=" << input_route_mode << "\n";
    oss << "FILTER_MODE_A=" << filter_mode_a << "\nFILTER_MODE_B=" << filter_mode_b << "\n";
    oss << "FILTER_CUTOFF_B=" << filter_cutoff_b_hz << "\nFILTER_RES_B=" << filter_resonance_b << "\n";
    oss << "FILTER_DRIVE=" << filter_drive << "\nFILTER_WET=" << filter_wet << "\nFILTER_ROUTING=" << filter_routing << "\n";
    oss << "FILTER_SLOPE_A=" << (filter_slope_a_24db?1:0) << "\nFILTER_SLOPE_B=" << (filter_slope_b_24db?1:0) << "\nFILTER_KEYTRACK=" << filter_key_tracking << "\n";
    for (size_t i=0;i<4;++i) oss << "EQ"<<i<<"_TYPE="<<eq_type[i]<<"\nEQ"<<i<<"_FREQ="<<eq_frequency_hz[i]<<"\nEQ"<<i<<"_GAIN="<<eq_gain_db[i]<<"\nEQ"<<i<<"_Q="<<eq_q[i]<<"\n";
    oss << "EQ_BYPASS="<<(eq_bypass?1:0)<<"\nEQ_GAIN_COMP="<<(eq_gain_compensation?1:0)<<"\n";
    oss << "LFO1_WAVE="<<lfo1_waveform<<"\nMOTION_CURVE="<<motion_curve<<"\n";
    oss << "MOD_LFO_CUTOFF="<<mod_lfo_cutoff<<"\nMOD_LFO_RES="<<mod_lfo_resonance<<"\nMOD_LFO_FM="<<mod_lfo_fm<<"\nMOD_LFO_SUB="<<mod_lfo_sub_blend<<"\nMOD_LFO_DRIVE="<<mod_lfo_drive<<"\nMOD_LFO_EQ_FREQ="<<mod_lfo_eq_frequency<<"\nMOD_LFO_EQ_GAIN="<<mod_lfo_eq_gain<<"\n";
    oss << "MOD_STATE_DIR_FILTER="<<mod_state_direction_filter<<"\nMOD_STATE_FAST_FM="<<mod_state_fast_fm<<"\nMOD_STATE_SLOW_BAL="<<mod_state_slow_balance<<"\nMOD_STATE_SLOW_RESO="<<mod_state_slow_resonator<<"\nMOD_AUDIO_ENV_DRIVE="<<mod_audio_envelope_drive<<"\n";
    oss << "MONO="<<(mono_mode?1:0)<<"\nLEGATO="<<(legato?1:0)<<"\nPORTAMENTO="<<portamento_seconds<<"\nBEND_RANGE="<<pitch_bend_range<<"\nVIBRATO="<<vibrato_depth_semitones<<"\nVELOCITY_TONE="<<velocity_tone<<"\nAFTERTOUCH_FILTER="<<aftertouch_filter<<"\nAFTERTOUCH_DRIVE="<<aftertouch_drive<<"\n";
    const auto& s=sound_space;
    oss<<"SPACE_ENABLED="<<(s.enabled?1:0)<<"\nSPACE_PITCH_BOUND="<<s.pitch_bound_cents<<"\nSPACE_HARM_BOUND="<<s.harmonic_bound_cents<<"\nSPACE_MODAL_BOUND="<<s.modal_bound_cents<<"\n";
    oss<<"SPACE_RATE="<<s.movement_hz<<"\nSPACE_ATTRACT="<<s.attraction<<"\nSPACE_RESIST="<<s.resistance<<"\nSPACE_REPEL="<<s.repulsion<<"\nSPACE_FREQ_FREEDOM="<<s.frequency_freedom<<"\nSPACE_ENERGY_WIDEN="<<s.energy_widen<<"\n";
    oss<<"SPACE_ATTACK="<<s.attack_freedom<<"\nSPACE_RELEASE="<<s.release_relaxation<<"\nSPACE_PITCH_MIX="<<s.pitch_mix<<"\nSPACE_HARM_MIX="<<s.harmonic_mix<<"\nSPACE_MODAL_MIX="<<s.modal_mix<<"\nSPACE_SPECTRAL_DB="<<s.spectral_depth_db<<"\nSPACE_PRIORITY="<<s.spectral_priority<<"\nSPACE_PHASE="<<s.phase_offset_cycles<<"\nSPACE_PHASE_COUPLE="<<s.phase_coupling<<"\n";
    for(size_t i=0;i<s.routes.size();++i)oss<<"SPACE_ROUTE"<<i<<"_SOURCE="<<s.routes[i].source<<"\nSPACE_ROUTE"<<i<<"_DEST="<<s.routes[i].destination<<"\nSPACE_ROUTE"<<i<<"_DEPTH="<<s.routes[i].depth<<"\n";
    const auto& v=vocal_expression;
    oss<<"VOCAL_ENABLED="<<(v.enabled?1:0)<<"\nVOCAL_CORRECTION="<<v.correction_strength<<"\nVOCAL_DRIFT="<<v.drift_retention<<"\nVOCAL_VIBRATO="<<v.vibrato_retention<<"\nVOCAL_TRANSITION="<<v.transition<<"\nVOCAL_FORMANT="<<v.formant_repair<<"\nVOCAL_RESIDUAL="<<v.spectral_residual_mix<<"\nVOCAL_CHARACTER="<<v.character<<"\nVOCAL_MIX="<<v.mix<<"\n";
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
        else if (key == "FUND_MIX") fundamental_mix = std::stof(val_str);
        else if (key == "SUB_RATIO") sub_ratio_denominator = std::stoi(val_str);
        else if (key == "SUB_PHASE") sub_phase = std::stof(val_str);
        else if (key == "SUB_POLARITY") sub_polarity_inverted = std::stoi(val_str)!=0;
        else if (key == "SUB_SAT") sub_saturation = std::stof(val_str);
        else if (key == "SUB_ENV") sub_envelope_amount = std::stof(val_str);
        else if (key == "SOURCE_LEVEL") source_level = std::stof(val_str);
        else if (key == "CHAR_LEVEL") character_level = std::stof(val_str);
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
        else if (key == "OSC_FM") osc_fm_amount = std::stof(val_str);
        else if (key == "OSC2_SEMI") osc2_semi = std::stoi(val_str);
        else if (key == "OSC_SYNC") osc_hard_sync = (std::stoi(val_str) != 0);
        else if (key == "STATE_RESIST") state_resistance = std::stof(val_str);
        else if (key == "STATE_REPEL") state_repulsion = std::stof(val_str);
        else if (key == "STATE_COUPLE") state_coupling = std::stof(val_str);
        else if (key == "STATE_PERSIST") state_persistence = std::stof(val_str);
        else if (key == "STATE_EN") state_enabled = (std::stoi(val_str) != 0);
        else if (key == "LFO1_RATE") lfo1_rate_hz = std::stof(val_str);
        else if (key == "LFO1_DEPTH") lfo1_depth = std::stof(val_str);
        else if (key == "ROUTE_MODE") input_route_mode = std::stoi(val_str);
        else if (key == "FILTER_MODE_A") filter_mode_a=std::stoi(val_str);
        else if (key == "FILTER_MODE_B") filter_mode_b=std::stoi(val_str);
        else if (key == "FILTER_CUTOFF_B") filter_cutoff_b_hz=std::stof(val_str);
        else if (key == "FILTER_RES_B") filter_resonance_b=std::stof(val_str);
        else if (key == "FILTER_DRIVE") filter_drive=std::stof(val_str);
        else if (key == "FILTER_WET") filter_wet=std::stof(val_str);
        else if (key == "FILTER_ROUTING") filter_routing=std::stoi(val_str);
        else if (key == "FILTER_SLOPE_A") filter_slope_a_24db=std::stoi(val_str)!=0;
        else if (key == "FILTER_SLOPE_B") filter_slope_b_24db=std::stoi(val_str)!=0;
        else if (key == "FILTER_KEYTRACK") filter_key_tracking=std::stof(val_str);
        else if (key.rfind("EQ",0)==0 && key.size()>3 && key[3]=='_') {
            int i=key[2]-'0'; if(i>=0&&i<4){ auto suffix=key.substr(4); if(suffix=="TYPE")eq_type[i]=std::stoi(val_str); else if(suffix=="FREQ")eq_frequency_hz[i]=std::stof(val_str); else if(suffix=="GAIN")eq_gain_db[i]=std::stof(val_str); else if(suffix=="Q")eq_q[i]=std::stof(val_str); }
        }
        else if (key == "EQ_BYPASS") eq_bypass=std::stoi(val_str)!=0;
        else if (key == "EQ_GAIN_COMP") eq_gain_compensation=std::stoi(val_str)!=0;
        else if (key == "LFO1_WAVE") lfo1_waveform=std::stoi(val_str);
        else if (key == "MOTION_CURVE") motion_curve=std::stof(val_str);
        else if (key == "MOD_LFO_CUTOFF") mod_lfo_cutoff=std::stof(val_str);
        else if (key == "MOD_LFO_RES") mod_lfo_resonance=std::stof(val_str);
        else if (key == "MOD_LFO_FM") mod_lfo_fm=std::stof(val_str);
        else if (key == "MOD_LFO_SUB") mod_lfo_sub_blend=std::stof(val_str);
        else if (key == "MOD_LFO_DRIVE") mod_lfo_drive=std::stof(val_str);
        else if (key == "MOD_LFO_EQ_FREQ") mod_lfo_eq_frequency=std::stof(val_str);
        else if (key == "MOD_LFO_EQ_GAIN") mod_lfo_eq_gain=std::stof(val_str);
        else if (key == "MOD_STATE_DIR_FILTER") mod_state_direction_filter=std::stof(val_str);
        else if (key == "MOD_STATE_FAST_FM") mod_state_fast_fm=std::stof(val_str);
        else if (key == "MOD_STATE_SLOW_BAL") mod_state_slow_balance=std::stof(val_str);
        else if (key == "MOD_STATE_SLOW_RESO") mod_state_slow_resonator=std::stof(val_str);
        else if (key == "MOD_AUDIO_ENV_DRIVE") mod_audio_envelope_drive=std::stof(val_str);
        else if (key == "MONO") mono_mode=std::stoi(val_str)!=0;
        else if (key == "LEGATO") legato=std::stoi(val_str)!=0;
        else if (key == "PORTAMENTO") portamento_seconds=std::stof(val_str);
        else if (key == "BEND_RANGE") pitch_bend_range=std::stof(val_str);
        else if (key == "VIBRATO") vibrato_depth_semitones=std::stof(val_str);
        else if (key == "VELOCITY_TONE") velocity_tone=std::stof(val_str);
        else if (key == "AFTERTOUCH_FILTER") aftertouch_filter=std::stof(val_str);
        else if (key == "AFTERTOUCH_DRIVE") aftertouch_drive=std::stof(val_str);
        else if(key=="SPACE_ENABLED")sound_space.enabled=std::stoi(val_str)!=0;
        else if(key=="SPACE_PITCH_BOUND")sound_space.pitch_bound_cents=std::stof(val_str);
        else if(key=="SPACE_HARM_BOUND")sound_space.harmonic_bound_cents=std::stof(val_str);
        else if(key=="SPACE_MODAL_BOUND")sound_space.modal_bound_cents=std::stof(val_str);
        else if(key=="SPACE_RATE")sound_space.movement_hz=std::stof(val_str);
        else if(key=="SPACE_ATTRACT")sound_space.attraction=std::stof(val_str);
        else if(key=="SPACE_RESIST")sound_space.resistance=std::stof(val_str);
        else if(key=="SPACE_REPEL")sound_space.repulsion=std::stof(val_str);
        else if(key=="SPACE_FREQ_FREEDOM")sound_space.frequency_freedom=std::stof(val_str);
        else if(key=="SPACE_ENERGY_WIDEN")sound_space.energy_widen=std::stof(val_str);
        else if(key=="SPACE_ATTACK")sound_space.attack_freedom=std::stof(val_str);
        else if(key=="SPACE_RELEASE")sound_space.release_relaxation=std::stof(val_str);
        else if(key=="SPACE_PITCH_MIX")sound_space.pitch_mix=std::stof(val_str);
        else if(key=="SPACE_HARM_MIX")sound_space.harmonic_mix=std::stof(val_str);
        else if(key=="SPACE_MODAL_MIX")sound_space.modal_mix=std::stof(val_str);
        else if(key=="SPACE_SPECTRAL_DB")sound_space.spectral_depth_db=std::stof(val_str);
        else if(key=="SPACE_PRIORITY")sound_space.spectral_priority=std::stof(val_str);
        else if(key=="SPACE_PHASE")sound_space.phase_offset_cycles=std::stof(val_str);
        else if(key=="SPACE_PHASE_COUPLE")sound_space.phase_coupling=std::stof(val_str);
        else if(key.rfind("SPACE_ROUTE",0)==0){int i=key[11]-'0';if(i>=0&&i<4){auto suffix=key.substr(13);if(suffix=="SOURCE")sound_space.routes[i].source=std::stoi(val_str);else if(suffix=="DEST")sound_space.routes[i].destination=std::stoi(val_str);else if(suffix=="DEPTH")sound_space.routes[i].depth=std::stof(val_str);}}
        else if(key=="VOCAL_ENABLED")vocal_expression.enabled=std::stoi(val_str)!=0;
        else if(key=="VOCAL_CORRECTION")vocal_expression.correction_strength=std::stof(val_str);
        else if(key=="VOCAL_DRIFT")vocal_expression.drift_retention=std::stof(val_str);
        else if(key=="VOCAL_VIBRATO")vocal_expression.vibrato_retention=std::stof(val_str);
        else if(key=="VOCAL_TRANSITION")vocal_expression.transition=std::stof(val_str);
        else if(key=="VOCAL_FORMANT")vocal_expression.formant_repair=std::stof(val_str);
        else if(key=="VOCAL_RESIDUAL")vocal_expression.spectral_residual_mix=std::stof(val_str);
        else if(key=="VOCAL_CHARACTER")vocal_expression.character=std::stof(val_str);
        else if(key=="VOCAL_MIX")vocal_expression.mix=std::stof(val_str);
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
    p.macros[MACRO_CUTOFF] = 0.72f;
    p.macros[MACRO_RESONANCE] = 0.45f;
    p.macros[MACRO_DRIVE] = 0.55f;
    p.macros[MACRO_BODY] = 0.50f;
    p.macros[MACRO_DELAY] = 0.35f;
    p.macros[MACRO_SPACE] = 0.40f;
    p.macros[MACRO_MIC_BLEND] = 0.0f;
    p.macros[MACRO_CHARACTER] = 0.60f;
    p.synth_waveform = 0; // Saw
    p.osc_fm_amount = 0.25f;
    p.osc2_semi = 7;      // Perfect fifth modulator
    p.osc_hard_sync = true;
    p.state_resistance = 0.30f;
    p.state_repulsion = 0.45f;
    p.state_coupling = 0.40f;
    p.state_enabled = true;
    p.amp_attack = 0.008f;
    p.amp_decay = 0.200f;
    p.amp_sustain = 0.85f;
    p.amp_release = 0.300f;
    p.filter_env_amount = 0.65f;
    p.input_route_mode = 0;
    return p;
}

PresetData PresetManager::create_factory_bass() {
    PresetData p;
    p.name = "Sub Harmonic Beast";
    p.macros[MACRO_CUTOFF] = 0.38f;
    p.macros[MACRO_RESONANCE] = 0.30f;
    p.macros[MACRO_DRIVE] = 0.65f;
    p.macros[MACRO_BODY] = 0.70f;
    p.macros[MACRO_DELAY] = 0.0f;
    p.macros[MACRO_SPACE] = 0.15f;
    p.macros[MACRO_MIC_BLEND] = 0.0f;
    p.macros[MACRO_CHARACTER] = 0.70f;
    p.synth_waveform = 1; // Pulse
    p.sub_mix = 0.60f;
    p.osc_fm_amount = 0.10f;
    p.osc2_semi = -12;    // Sub octave modulator
    p.state_resistance = 0.60f; // Stiff history-dependent resistance
    p.state_repulsion = 0.50f;
    p.state_coupling = 0.20f;
    p.state_enabled = true;
    p.amp_attack = 0.002f;
    p.amp_decay = 0.350f;
    p.amp_sustain = 0.50f;
    p.amp_release = 0.150f;
    p.filter_env_amount = 0.40f;
    p.input_route_mode = 0;
    return p;
}

PresetData PresetManager::create_factory_stateful_bell() {
    PresetData p;
    p.name = "Chronofrequency Inharmonic Bell";
    p.macros[MACRO_CUTOFF] = 0.85f;
    p.macros[MACRO_RESONANCE] = 0.60f;
    p.macros[MACRO_DRIVE] = 0.20f;
    p.macros[MACRO_BODY] = 0.90f;
    p.macros[MACRO_DELAY] = 0.40f;
    p.macros[MACRO_SPACE] = 0.65f;
    p.macros[MACRO_MIC_BLEND] = 0.0f;
    p.macros[MACRO_CHARACTER] = 0.40f;
    p.synth_waveform = 3; // Sine carrier
    p.osc_fm_amount = 0.68f;
    p.osc2_semi = 19;     // Octave + fifth ratio
    p.osc_hard_sync = false;
    p.state_resistance = 0.25f;
    p.state_repulsion = 0.85f; // High negative gravity boundary repulsion
    p.state_coupling = 0.75f;  // Strong signed resonance coupling
    p.state_persistence = 0.80f; // Long macro acoustic memory
    p.state_enabled = true;
    p.amp_attack = 0.001f;
    p.amp_decay = 1.200f;
    p.amp_sustain = 0.10f;
    p.amp_release = 0.800f;
    p.filter_env_amount = 0.70f;
    p.input_route_mode = 0;
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
    p.state_resistance = 0.45f;
    p.state_repulsion = 0.40f;
    p.state_coupling = 0.50f;
    p.state_enabled = true;
    p.input_route_mode = 2; // External audio primary
    p.vocal_expression={true,.72f,.62f,.82f,.68f,.58f,.10f,.24f,.88f};
    return p;
}

PresetData PresetManager::create_factory_guitar_processor() {
    PresetData p;
    p.name = "Live Guitar Tube & Cab Chamber";
    p.macros[MACRO_CUTOFF] = 0.75f;
    p.macros[MACRO_RESONANCE] = 0.35f;
    p.macros[MACRO_DRIVE] = 0.70f;     // Warm screaming tube saturation
    p.macros[MACRO_BODY] = 0.80f;      // Heavy 4x12 cabinet acoustic presence
    p.macros[MACRO_DELAY] = 0.30f;
    p.macros[MACRO_SPACE] = 0.45f;
    p.macros[MACRO_MIC_BLEND] = 1.0f;
    p.macros[MACRO_CHARACTER] = 0.75f; // Dynamic cathode sag for pick touch sensitivity
    p.state_resistance = 0.50f;
    p.state_repulsion = 0.60f;
    p.state_coupling = 0.45f;
    p.state_enabled = true;
    p.input_route_mode = 2; // External guitar input
    return p;
}

PresetData PresetManager::create_monolith() {
    PresetData p = create_factory_bass(); p.name="MONOLITH";
    p.synth_waveform=3; p.fundamental_mix=0.72f; p.sub_mix=0.58f; p.sub_ratio_denominator=2;
    p.sub_saturation=0.22f; p.sub_envelope_amount=0.82f; p.character_level=0.30f;
    p.macros[MACRO_CUTOFF]=0.32f; p.macros[MACRO_DRIVE]=0.28f; p.macros[MACRO_BODY]=0.18f;
    p.filter_mode_a=0; p.filter_mode_b=0; p.filter_cutoff_b_hz=145.0f; p.filter_slope_a_24db=true;
    p.filter_routing=0; p.eq_type={{2,1,1,5}}; p.eq_frequency_hz={{48,180,720,6200}};
    p.eq_gain_db={{2.5f,-2.0f,1.0f,0}}; p.eq_q={{0.7f,1.0f,0.8f,0.7f}};
    p.state_resistance=0.72f; p.state_coupling=0.14f; p.mod_state_slow_balance=0.18f;
    p.sound_space.enabled=true;p.sound_space.harmonic_bound_cents=18;p.sound_space.harmonic_mix=.13f;p.sound_space.frequency_freedom=.85f;
    p.sound_space.spectral_depth_db=7.0f;p.sound_space.spectral_priority=.82f;p.sound_space.resistance=.78f;
    p.sound_space.routes[0]={2,1,.35f};p.macros[MACRO_DELAY]=0; p.macros[MACRO_SPACE]=0.05f; p.master_gain_db=-6.0f; return p;
}

PresetData PresetManager::create_feral_wobble() {
    PresetData p=create_factory_bass(); p.name="FERAL WOBBLE"; p.synth_waveform=0;
    p.fundamental_mix=0.28f; p.sub_mix=0.42f; p.sub_ratio_denominator=2; p.sub_saturation=0.40f;
    p.character_level=0.88f; p.filter_mode_a=0; p.filter_mode_b=2; p.filter_cutoff_b_hz=92.0f;
    p.filter_resonance_b=0.20f; p.filter_drive=0.62f; p.filter_routing=2; p.filter_slope_a_24db=true;
    p.lfo1_rate_hz=3.25f; p.lfo1_depth=1.0f; p.lfo1_waveform=1; p.motion_curve=0.58f;
    p.mod_lfo_cutoff=0.78f; p.mod_lfo_resonance=0.24f; p.mod_lfo_fm=0.36f;
    p.mod_lfo_sub_blend=-0.22f; p.mod_lfo_drive=0.24f; p.mod_lfo_eq_frequency=0.32f;
    p.mod_state_direction_filter=0.38f; p.mod_state_fast_fm=0.34f; p.mod_state_slow_balance=0.30f;
    p.mod_state_slow_resonator=0.24f; p.eq_type={{4,1,1,3}}; p.eq_frequency_hz={{28,170,1450,7200}};
    p.eq_gain_db={{0,-2.5f,3.5f,1.0f}}; p.eq_q={{0.7f,1.2f,1.5f,0.7f}};
    p.sound_space.enabled=true;p.sound_space.pitch_bound_cents=16;p.sound_space.harmonic_bound_cents=42;p.sound_space.modal_bound_cents=31;p.sound_space.movement_hz=2.1f;p.sound_space.attraction=.38f;p.sound_space.resistance=.48f;p.sound_space.repulsion=.72f;p.sound_space.harmonic_mix=.22f;p.sound_space.modal_mix=.16f;p.sound_space.phase_offset_cycles=.10f;p.sound_space.phase_coupling=.55f;p.sound_space.spectral_depth_db=5.0f;
    p.sound_space.routes[0]={0,1,.75f};p.sound_space.routes[1]={2,3,.50f};p.sound_space.routes[2]={4,6,.45f};p.master_gain_db=-9.0f; return p;
}

PresetData PresetManager::create_velvet_lead() {
    PresetData p=create_factory_lead(); p.name="VELVET LEAD"; p.synth_waveform=2;
    p.fundamental_mix=0.16f; p.sub_mix=0.08f; p.character_level=0.78f; p.mono_mode=true; p.legato=true;
    p.portamento_seconds=0.085f; p.pitch_bend_range=12.0f; p.vibrato_depth_semitones=0.28f;
    p.velocity_tone=0.42f; p.aftertouch_filter=0.55f; p.aftertouch_drive=0.26f;
    p.amp_attack=0.012f; p.amp_sustain=0.90f; p.amp_release=0.58f; p.osc_fm_amount=0.08f;
    p.osc_hard_sync=false; p.filter_mode_a=0; p.filter_mode_b=0; p.filter_cutoff_b_hz=7600.0f;
    p.filter_resonance_b=0.08f; p.filter_drive=0.18f; p.filter_slope_b_24db=true;
    p.eq_type={{4,1,1,3}}; p.eq_frequency_hz={{38,320,2100,7200}}; p.eq_gain_db={{0,-1.2f,2.2f,-1.5f}};
    p.eq_q={{0.7f,0.9f,0.8f,0.7f}}; p.state_resistance=0.42f; p.state_coupling=0.34f;
    p.mod_state_direction_filter=0.10f; p.mod_state_slow_resonator=0.16f; p.macros[MACRO_DRIVE]=0.38f;
    p.sound_space.enabled=true;p.sound_space.pitch_bound_cents=24;p.sound_space.harmonic_bound_cents=14;p.sound_space.modal_bound_cents=9;p.sound_space.movement_hz=.42f;p.sound_space.attraction=.82f;p.sound_space.resistance=.74f;p.sound_space.repulsion=.58f;p.sound_space.pitch_mix=.08f;p.sound_space.harmonic_mix=.10f;p.sound_space.modal_mix=.06f;p.sound_space.attack_freedom=.75f;p.sound_space.release_relaxation=.85f;
    p.sound_space.routes[0]={1,0,.72f};p.sound_space.routes[1]={5,0,.65f};p.sound_space.routes[2]={5,4,.35f};
    p.macros[MACRO_DELAY]=0.22f; p.macros[MACRO_SPACE]=0.20f; p.master_gain_db=-7.0f; return p;
}

PresetData PresetManager::create_sound_space(){
    PresetData p=create_factory_stateful_bell();p.name="SOUND SPACE";p.synth_waveform=3;p.fundamental_mix=.36f;p.sub_mix=.18f;p.character_level=.48f;p.osc_fm_amount=.10f;
    p.sound_space.enabled=true;p.sound_space.pitch_bound_cents=19;p.sound_space.harmonic_bound_cents=58;p.sound_space.modal_bound_cents=44;p.sound_space.movement_hz=.73f;p.sound_space.attraction=.61f;p.sound_space.resistance=.59f;p.sound_space.repulsion=.68f;p.sound_space.frequency_freedom=.88f;p.sound_space.energy_widen=.55f;p.sound_space.attack_freedom=.42f;p.sound_space.release_relaxation=.72f;p.sound_space.pitch_mix=.09f;p.sound_space.harmonic_mix=.20f;p.sound_space.modal_mix=.18f;p.sound_space.spectral_depth_db=8.5f;p.sound_space.spectral_priority=.66f;p.sound_space.phase_offset_cycles=.08f;p.sound_space.phase_coupling=.46f;
    p.sound_space.routes[0]={0,1,.55f};p.sound_space.routes[1]={3,2,.62f};p.sound_space.routes[2]={2,5,.48f};p.sound_space.routes[3]={5,6,.72f};
    p.lfo1_rate_hz=.37f;p.lfo1_depth=1.0f;p.master_gain_db=-9.0f;return p;
}

} // namespace monkeys_ear
