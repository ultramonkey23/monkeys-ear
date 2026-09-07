#include "monkeys_ear/engine.h"
#include <cmath>

namespace monkeys_ear {

MonkeysEarEngine::MonkeysEarEngine()
    : sample_rate_(48000.0f),
      max_block_size_(512),
      master_volume_(0.85f), aftertouch_(0.0f), audio_envelope_(0.0f),
      current_midi_note_(60), last_velocity_(0.5f), modulation_counter_(0) {
    init(sample_rate_, max_block_size_);
}

void MonkeysEarEngine::init(float sample_rate, size_t max_block_size) {
    sample_rate_ = sample_rate > 0.0f ? sample_rate : 48000.0f;
    max_block_size_ = max_block_size > 0 ? max_block_size : 512;

    voice_manager_.set_sample_rate(sample_rate_);
    audio_input_.set_sample_rate(sample_rate_);
    chrono_body_.set_sample_rate(sample_rate_);
    lfo1_.set_sample_rate(sample_rate_);
    filter_.set_sample_rate(sample_rate_);
    weight_lowpass_.set_sample_rate(sample_rate_);
    eq_.set_sample_rate(sample_rate_);
    external_sub_.set_sample_rate(sample_rate_);
    sound_space_.set_sample_rate(sample_rate_);
    cutoff_motion_.set_sample_rate(sample_rate_, 7.0f);
    resonance_motion_.set_sample_rate(sample_rate_, 10.0f);
    fm_motion_.set_sample_rate(sample_rate_, 5.0f);
    sub_motion_.set_sample_rate(sample_rate_, 12.0f);
    drive_motion_.set_sample_rate(sample_rate_, 10.0f);
    drive_tube_.set_sample_rate(sample_rate_);
    resonator_cab_.set_sample_rate(sample_rate_);
    delay_.set_sample_rate(sample_rate_);
    fdn_reverb_.set_sample_rate(sample_rate_);
    latency_meter_.set_sample_rate(sample_rate_);
    latency_meter_.set_block_size(max_block_size_);

    apply_macros();
}

void MonkeysEarEngine::reset() {
    voice_manager_.all_notes_off();
    audio_input_.reset();
    chrono_body_.reset();
    lfo1_.reset_phase();
    filter_.reset();
    weight_lowpass_.reset();
    eq_.reset();
    external_sub_.reset();
    sound_space_.reset();
    drive_tube_.reset();
    resonator_cab_.reset();
    delay_.reset();
    fdn_reverb_.reset();
    safety_limiter_.reset();
    latency_meter_.reset();
}

void MonkeysEarEngine::apply_macros() {
    const PresetData& p = preset_manager_.get_current();

    // Macro 1: Brightness / Filter Cutoff (exponential map 40Hz to 18000Hz)
    float m_cutoff = p.macros[MACRO_CUTOFF];
    float cutoff_hz = 40.0f * std::pow(450.0f, m_cutoff);
    float key_ratio = std::pow(2.0f, p.filter_key_tracking * (static_cast<float>(current_midi_note_)-60.0f)/12.0f);
    cutoff_hz = clamp(cutoff_hz * key_ratio, 20.0f, sample_rate_*0.45f);

    // Macro 2: Bite / Filter Resonance (0.05 to 0.92)
    float m_res = p.macros[MACRO_RESONANCE];
    float resonance_a = 0.05f + m_res * 0.85f;
    filter_.set_stage(0, static_cast<FilterMode>(clamp(p.filter_mode_a,0,4)), cutoff_hz,
                      resonance_a, p.filter_drive, p.filter_slope_a_24db);
    filter_.set_stage(1, static_cast<FilterMode>(clamp(p.filter_mode_b,0,4)), p.filter_cutoff_b_hz,
                      p.filter_resonance_b, p.filter_drive, p.filter_slope_b_24db);
    filter_.set_routing(static_cast<FilterRouting>(clamp(p.filter_routing,0,2)));
    filter_.set_mix(p.filter_wet);
    weight_lowpass_.set_mode(FilterMode::Lowpass);
    weight_lowpass_.set_cutoff(clamp(std::min(180.0f, cutoff_hz), 55.0f, 180.0f));
    weight_lowpass_.set_resonance(0.05f);

    // Macro 3: Heat / Tube Saturation (0.0 to 1.0)
    float m_drive = p.macros[MACRO_DRIVE];
    drive_tube_.set_drive(m_drive);
    drive_tube_.set_mix(0.2f + m_drive * 0.8f);

    // Macro 4: Body / Cabinet Resonance
    float m_body = p.macros[MACRO_BODY];
    resonator_cab_.set_body_size(0.6f + m_body * 1.4f);
    resonator_cab_.set_resonance(0.1f + m_body * 0.7f);
    resonator_cab_.set_mix(m_body * 0.75f);

    // Macro 5: Delay (time 120ms to 650ms, mix 0.0 to 0.65)
    float m_delay = p.macros[MACRO_DELAY];
    delay_.set_time_left(0.15f + m_delay * 0.35f);
    delay_.set_time_right(0.22f + m_delay * 0.48f);
    delay_.set_feedback(0.20f + m_delay * 0.55f);
    delay_.set_mix(m_delay * 0.65f);

    // Macro 6: Space / FDN Reverb Depth & Decay
    float m_space = p.macros[MACRO_SPACE];
    fdn_reverb_.set_room_size(0.6f + m_space * 1.4f);
    fdn_reverb_.set_decay_time(0.5f + m_space * 6.0f);
    fdn_reverb_.set_mix(m_space * 0.70f);

    // Macro 7: Mic Blend (0.0 = pure synth, 1.0 = pure mic)
    float m_mic = p.macros[MACRO_MIC_BLEND];
    audio_input_.set_mix(m_mic);
    audio_input_.set_vocal_controls(p.vocal_expression);

    // Macro 8: Dynamic Character / Cathode Sag
    float m_char = p.macros[MACRO_CHARACTER];
    drive_tube_.set_memory_sag(m_char);

    // Master volume from preset
    master_volume_ = std::pow(10.0f, p.master_gain_db / 20.0f) * 0.85f;

    // Advanced Sound Construction: Oscillators
    voice_manager_.set_waveform(static_cast<Waveform>(p.synth_waveform));
    voice_manager_.set_sub_mix(p.sub_mix);
    voice_manager_.set_fundamental_mix(p.fundamental_mix);
    voice_manager_.set_sub_ratio(p.sub_ratio_denominator);
    voice_manager_.set_sub_phase(p.sub_phase);
    voice_manager_.set_sub_polarity(p.sub_polarity_inverted);
    voice_manager_.set_sub_saturation(p.sub_saturation);
    voice_manager_.set_sub_envelope(p.sub_envelope_amount);
    voice_manager_.set_noise_mix(p.noise_mix);
    voice_manager_.set_fm_amount(p.osc_fm_amount);
    voice_manager_.set_osc2_semi(p.osc2_semi);
    voice_manager_.set_hard_sync(p.osc_hard_sync);
    voice_manager_.set_amp_envelope(p.amp_attack, p.amp_decay, p.amp_sustain, p.amp_release);
    voice_manager_.set_filter_envelope(p.filter_attack, p.filter_decay, p.filter_sustain, p.filter_release);
    voice_manager_.set_voice_mode(p.mono_mode, p.legato);
    voice_manager_.set_portamento(p.portamento_seconds);

    // Cody's Chrono-Stateful Resonant Body
    chrono_body_.set_resistance(p.state_resistance);
    chrono_body_.set_repulsion(p.state_repulsion);
    chrono_body_.set_coupling(p.state_coupling);
    chrono_body_.set_memory_persistence(p.state_persistence);
    chrono_body_.set_enabled(p.state_enabled);
    chrono_body_.set_frequency(cutoff_hz * 0.75f); // Modal body tuned relative to cutoff
    chrono_body_.set_mix(0.60f);

    // Modulation
    lfo1_.set_rate_hz(p.lfo1_rate_hz);
    lfo1_.set_depth(p.lfo1_depth);
    lfo1_.set_waveform(static_cast<LFOWaveform>(clamp(p.lfo1_waveform,0,4)));
    external_sub_.set_ratio(p.sub_ratio_denominator);
    external_sub_.set_phase(p.sub_phase);
    external_sub_.set_polarity(p.sub_polarity_inverted);
    external_sub_.set_saturation(p.sub_saturation);
    for (size_t i=0;i<ParametricEQ::NUM_BANDS;++i) {
        eq_.set_band(i, {static_cast<EQType>(clamp(p.eq_type[i],0,5)), p.eq_frequency_hz[i], p.eq_gain_db[i], p.eq_q[i]});
    }
    eq_.set_bypass(p.eq_bypass);
    eq_.set_gain_compensation(p.eq_gain_compensation);
    sound_space_.set_controls(p.sound_space);
}

void MonkeysEarEngine::set_macro(MacroId id, float value) {
    preset_manager_.set_macro(id, value);
    apply_macros();
}

float MonkeysEarEngine::get_macro(MacroId id) const {
    return preset_manager_.get_macro(id);
}

void MonkeysEarEngine::load_preset(const PresetData& preset) {
    preset_manager_.load_preset(preset);
    apply_macros();
}

const PresetData& MonkeysEarEngine::get_current_preset() const {
    return preset_manager_.get_current();
}

void MonkeysEarEngine::set_master_gain_db(float db) {
    preset_manager_.mutable_current().master_gain_db = clamp(db, -100.0f, 12.0f); apply_macros();
}

void MonkeysEarEngine::set_waveform(Waveform wf) {
    preset_manager_.mutable_current().synth_waveform = static_cast<int>(wf); apply_macros();
}

void MonkeysEarEngine::set_osc_fm(float fm) {
    preset_manager_.mutable_current().osc_fm_amount = clamp(fm,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_osc2_semi(int semi) {
    preset_manager_.mutable_current().osc2_semi = clamp(semi,-24,24); apply_macros();
}

void MonkeysEarEngine::set_osc_hard_sync(bool sync) {
    preset_manager_.mutable_current().osc_hard_sync = sync; apply_macros();
}

void MonkeysEarEngine::set_state_resistance(float r) {
    preset_manager_.mutable_current().state_resistance = clamp(r,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_state_repulsion(float k) {
    preset_manager_.mutable_current().state_repulsion = clamp(k,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_state_coupling(float kappa) {
    preset_manager_.mutable_current().state_coupling = clamp(kappa,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_state_persistence(float tau) {
    preset_manager_.mutable_current().state_persistence = clamp(tau,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_state_enabled(bool en) {
    preset_manager_.mutable_current().state_enabled = en; apply_macros();
}

void MonkeysEarEngine::set_lfo1_rate(float hz) {
    preset_manager_.mutable_current().lfo1_rate_hz = clamp(hz,0.05f,30.0f); apply_macros();
}

void MonkeysEarEngine::set_lfo1_depth(float depth) {
    preset_manager_.mutable_current().lfo1_depth = clamp(depth,0.0f,1.0f); apply_macros();
}

void MonkeysEarEngine::set_input_route_mode(int mode) {
    preset_manager_.mutable_current().input_route_mode = clamp(mode,0,2); apply_macros();
}

void MonkeysEarEngine::set_aftertouch(float value) { aftertouch_ = clamp(value,0.0f,1.0f); }

void MonkeysEarEngine::set_parameter_normalized(int id, float v) {
    v=clamp(v,0.0f,1.0f); auto& p=preset_manager_.mutable_current();
    if(id<NUM_MACROS){ preset_manager_.set_macro(static_cast<MacroId>(id),v); apply_macros(); return; }
    switch(id){
      case 8:p.master_gain_db=v>0.001f?(v-0.85f)*40.0f:-100.0f;break; case 9:p.synth_waveform=static_cast<int>(v*3.99f);break;
      case 10:p.osc_fm_amount=v;break; case 11:p.osc2_semi=static_cast<int>(std::round((v-.5f)*48));break; case 12:p.osc_hard_sync=v>=.5f;break;
      case 13:p.state_resistance=v;break;case 14:p.state_repulsion=v;break;case 15:p.state_coupling=v;break;case 16:p.state_persistence=v;break;case 17:p.state_enabled=v>=.5f;break;case 18:p.input_route_mode=static_cast<int>(v*2.99f);break;
      case 19:p.fundamental_mix=v;break;case 20:p.sub_mix=v;break;case 21:p.sub_ratio_denominator=1+static_cast<int>(v*3.99f);break;case 22:p.sub_phase=v;break;case 23:p.sub_polarity_inverted=v>=.5f;break;case 24:p.sub_saturation=v;break;case 25:p.sub_envelope_amount=v;break;case 26:p.source_level=v;break;case 27:p.character_level=v;break;
      case 28:p.filter_mode_a=static_cast<int>(v*4.99f);break;case 29:p.filter_cutoff_b_hz=40.0f*std::pow(450.0f,v);break;case 30:p.filter_mode_b=static_cast<int>(v*4.99f);break;case 31:p.filter_routing=static_cast<int>(v*2.99f);break;case 32:p.filter_wet=v;break;case 33:p.filter_key_tracking=v;break;case 34:p.filter_slope_a_24db=v>=.5f;break;case 35:p.filter_slope_b_24db=v>=.5f;break;case 36:p.filter_drive=v*2.0f;break;
      case 53:p.eq_bypass=v>=.5f;break;case 54:p.eq_gain_compensation=v>=.5f;break;case 55:p.lfo1_rate_hz=.05f*std::pow(600.0f,v);break;case 56:p.lfo1_waveform=static_cast<int>(v*4.99f);break;case 57:p.motion_curve=v;break;case 58:p.mod_lfo_cutoff=(v-.5f)*2;break;case 59:p.mod_lfo_resonance=(v-.5f)*2;break;case 60:p.mod_lfo_fm=(v-.5f)*2;break;case 61:p.mod_lfo_sub_blend=(v-.5f)*2;break;case 62:p.mod_lfo_drive=(v-.5f)*2;break;case 63:p.mod_lfo_eq_frequency=(v-.5f)*2;break;case 64:p.mod_lfo_eq_gain=(v-.5f)*2;break;case 65:p.mod_state_direction_filter=(v-.5f)*2;break;case 66:p.mod_state_fast_fm=(v-.5f)*2;break;case 67:p.mod_state_slow_balance=(v-.5f)*2;break;case 68:p.mod_state_slow_resonator=(v-.5f)*2;break;case 69:p.mod_audio_envelope_drive=(v-.5f)*2;break;
      case 70:p.mono_mode=v>=.5f;break;case 71:p.legato=v>=.5f;break;case 72:p.portamento_seconds=v*v*2.0f;break;case 73:p.pitch_bend_range=1.0f+v*23.0f;break;case 74:p.vibrato_depth_semitones=v*2.0f;break;case 75:p.velocity_tone=v;break;case 76:p.aftertouch_filter=(v-.5f)*2;break;case 77:p.aftertouch_drive=(v-.5f)*2;break;case 78:p.lfo1_depth=v;break;
      case 79:{int patch=static_cast<int>(v*3.99f);if(patch==1)load_preset(PresetManager::create_monolith());else if(patch==2)load_preset(PresetManager::create_feral_wobble());else if(patch==3)load_preset(PresetManager::create_velvet_lead());return;}
      case 80:handle_midi_pitch_bend((v-.5f)*2.0f*p.pitch_bend_range);return;
      case 81:set_aftertouch(v);return;
      case 82:p.sound_space.enabled=v>=.5f;break;
      case 83:p.sound_space.pitch_bound_cents=v*100.0f;break;
      case 84:p.sound_space.harmonic_bound_cents=v*100.0f;break;
      case 85:p.sound_space.modal_bound_cents=v*100.0f;break;
      case 86:p.sound_space.movement_hz=.02f*std::pow(1000.0f,v);break;
      case 87:p.sound_space.attraction=v;break;case 88:p.sound_space.resistance=v;break;case 89:p.sound_space.repulsion=v;break;
      case 90:p.sound_space.frequency_freedom=v;break;case 91:p.sound_space.energy_widen=(v-.5f)*2.0f;break;
      case 92:p.sound_space.attack_freedom=v;break;case 93:p.sound_space.release_relaxation=v;break;
      case 94:p.sound_space.pitch_mix=v;break;case 95:p.sound_space.harmonic_mix=v;break;case 96:p.sound_space.modal_mix=v;break;
      case 97:p.sound_space.spectral_depth_db=v*18.0f;break;case 98:p.sound_space.spectral_priority=v;break;
      case 99:p.sound_space.phase_offset_cycles=(v-.5f)*.5f;break;case 100:p.sound_space.phase_coupling=v;break;
      case 113:p.vocal_expression.enabled=v>=.5f;break;case 114:p.vocal_expression.correction_strength=v;break;case 115:p.vocal_expression.drift_retention=v;break;case 116:p.vocal_expression.vibrato_retention=v;break;case 117:p.vocal_expression.transition=v;break;case 118:p.vocal_expression.formant_repair=v;break;case 119:p.vocal_expression.spectral_residual_mix=v;break;case 120:p.vocal_expression.character=v;break;case 121:p.vocal_expression.mix=v;break;
      default:
        if(id>=37&&id<=52){int b=(id-37)/4, f=(id-37)%4; if(f==0)p.eq_type[b]=static_cast<int>(v*5.99f);else if(f==1)p.eq_frequency_hz[b]=20.0f*std::pow(1000.0f,v);else if(f==2)p.eq_gain_db[b]=(v-.5f)*36.0f;else p.eq_q[b]=.15f*std::pow(80.0f,v);}
        else if(id>=101&&id<=112){int route=(id-101)/3,field=(id-101)%3;if(field==0)p.sound_space.routes[route].source=static_cast<int>(v*7.99f);else if(field==1)p.sound_space.routes[route].destination=static_cast<int>(v*7.99f);else p.sound_space.routes[route].depth=(v-.5f)*2.0f;}
        break;
    } apply_macros();
}

void MonkeysEarEngine::handle_midi_note_on(int note, float velocity) {
    current_midi_note_ = note;
    last_velocity_ = clamp(velocity,0.0f,1.0f);
    apply_macros();
    voice_manager_.note_on(note, velocity);
}

void MonkeysEarEngine::handle_midi_note_off(int note) {
    voice_manager_.note_off(note);
}

void MonkeysEarEngine::handle_midi_pitch_bend(float semitones) {
    float range=preset_manager_.get_current().pitch_bend_range;
    voice_manager_.set_pitch_bend(clamp(semitones,-range,range));
}

void MonkeysEarEngine::handle_midi_cc(int cc_number, float value_0_to_1) {
    switch (cc_number) {
        case 1:  set_aftertouch(value_0_to_1); break;
        case 74: set_macro(MACRO_CUTOFF, value_0_to_1); break;
        case 71: set_macro(MACRO_RESONANCE, value_0_to_1); break;
        case 91: set_macro(MACRO_SPACE, value_0_to_1); break;
        case 93: set_macro(MACRO_DELAY, value_0_to_1); break;
        default: break;
    }
}

void MonkeysEarEngine::handle_all_notes_off() {
    voice_manager_.all_notes_off();
}

void MonkeysEarEngine::process_block(
    const float* input_l,
    const float* input_r,
    float* output_l,
    float* output_r,
    size_t num_samples
) {
    latency_meter_.set_block_size(num_samples);
    latency_meter_.start_block();

    const PresetData& p = preset_manager_.get_current();
    float env_amt = p.filter_env_amount;

    for (size_t i = 0; i < num_samples; ++i) {
        // 0. Modulation (LFO)
        float lfo_val = shape_modulation(lfo1_.process(), p.motion_curve);

        // 1. Synth Voices Generation
        float filter_env = 0.0f;
        const auto& prior_state=chrono_body_.get_state();
        float fast=clamp(std::sqrt(std::max(0.0f,prior_state.energy_fast))*2.0f,0.0f,1.0f);
        float fm_mod=fm_motion_.process(lfo_val*p.mod_lfo_fm + fast*p.mod_state_fast_fm);
        voice_manager_.set_fm_amount(clamp(p.osc_fm_amount + fm_mod*.45f,0.0f,1.0f));
        voice_manager_.set_vibrato(std::sin(static_cast<float>(modulation_counter_)*TWO_PI*5.3f/sample_rate_) * p.vibrato_depth_semitones + aftertouch_*p.vibrato_depth_semitones*.35f);
        float synth_weight=0.0f;
        float synth_sample = voice_manager_.process_split(filter_env, synth_weight);

        // 2. Live External Audio Ingest & Preamp/Conditioning
        float ext_sample = 0.0f;
        if (input_l != nullptr) {
            ext_sample = (input_r != nullptr) ? 0.5f * (input_l[i] + input_r[i]) : input_l[i];
        }
        float tracked_sub=external_sub_.process(ext_sample);
        float blended_ext = audio_input_.process_sample(ext_sample, synth_sample);
        blended_ext = audio_input_.process_vocal_sample(blended_ext, external_sub_.tracked_frequency_hz(), external_sub_.confidence());
        audio_envelope_ += (std::abs(ext_sample)-audio_envelope_) * (std::abs(ext_sample)>audio_envelope_?.02f:.0008f);

        // 3. Routing Mode Resolution
        float exciter = 0.0f;
        if (p.input_route_mode == 2) {
            // Pure External Audio Processor (Guitar / Mic / Recorded Audio)
            exciter = (input_l != nullptr) ? blended_ext : 0.0f;
        } else if (p.input_route_mode == 1) {
            // Hybrid Blend Mode
            exciter = blended_ext;
        } else {
            // Synth Instrument Primary (if mic blend macro > 0, blend in external audio)
            exciter = (p.macros[MACRO_MIC_BLEND] > 0.001f) ? blended_ext : synth_sample;
        }

        // 4. Cody's Chrono-Stateful Resonant Body
        // (Multiscale State -> Resistance -> Repulsion -> Signed Coupling)
        float stateful_source = chrono_body_.process_sample(exciter);

        const auto& state=chrono_body_.get_state();
        float direction=clamp(state.velocity/900.0f,-1.0f,1.0f);
        float slow=clamp(std::sqrt(std::max(0.0f,state.energy_slow))*2.0f,0.0f,1.0f);

        // 5. State Variable Filter (with envelope and LFO modulation)
        float filter_motion=cutoff_motion_.process(lfo_val*p.mod_lfo_cutoff + direction*p.mod_state_direction_filter + aftertouch_*p.aftertouch_filter + (last_velocity_-.5f)*p.velocity_tone);
        float resonance_mod=resonance_motion_.process(lfo_val*p.mod_lfo_resonance);
        float mod_env = filter_env*env_amt + filter_motion;
        float filtered = filter_.process(stateful_source, clamp(mod_env,-1.0f,1.0f), 1.0f);

        // 6. Tube Drive Stage (with dynamic cathode sag memory)
        float drive_mod=drive_motion_.process(lfo_val*p.mod_lfo_drive + audio_envelope_*p.mod_audio_envelope_drive + aftertouch_*p.aftertouch_drive);
        if((modulation_counter_++ & 15u)==0u){
            drive_tube_.set_drive(clamp(p.macros[MACRO_DRIVE]+drive_mod*.5f,0.0f,1.0f));
            resonator_cab_.set_body_size(clamp(.6f+p.macros[MACRO_BODY]*1.4f+slow*p.mod_state_slow_resonator,0.4f,2.2f));
            float base_cutoff=40.0f*std::pow(450.0f,p.macros[MACRO_CUTOFF]);
            base_cutoff*=std::pow(2.0f,p.filter_key_tracking*(static_cast<float>(current_midi_note_)-60.0f)/12.0f);
            filter_.set_stage(0,static_cast<FilterMode>(clamp(p.filter_mode_a,0,4)),base_cutoff,
                clamp(.05f+p.macros[MACRO_RESONANCE]*.85f+resonance_mod*.35f,0.0f,.98f),p.filter_drive,p.filter_slope_a_24db);
        }
        float driven = drive_tube_.process(filtered);

        // 7. Modal Cabinet Resonator
        float resonated = resonator_cab_.process(driven);

        float weight = synth_weight;
        if(p.input_route_mode==2) weight = weight_lowpass_.process(ext_sample) * p.fundamental_mix + tracked_sub*p.sub_mix;
        else if(p.input_route_mode==1) weight = lerp(synth_weight, weight_lowpass_.process(ext_sample)*p.fundamental_mix+tracked_sub*p.sub_mix, p.macros[MACRO_MIC_BLEND]);
        weight=weight_lowpass_.process(weight);
        float sub_move=sub_motion_.process(lfo_val*p.mod_lfo_sub_blend + slow*p.mod_state_slow_balance);
        float character_layer=resonated*p.character_level;
        float weight_layer=weight*p.source_level*clamp(1.0f+sub_move*.6f,0.1f,1.8f);
        float center_hz=midi_to_freq(static_cast<float>(current_midi_note_));
        if(p.input_route_mode!=0&&external_sub_.confidence()>.30f)center_hz=external_sub_.tracked_frequency_hz();
        SoundSpaceSources space_sources{lfo_val,filter_env,fast,slow,direction,aftertouch_,last_velocity_,audio_envelope_};
        float recombined=sound_space_.process(character_layer,weight_layer,center_hz,space_sources);
        float eq_freq=lfo_val*p.mod_lfo_eq_frequency*.75f;
        float eq_gain=lfo_val*p.mod_lfo_eq_gain*9.0f;
        float equalized=eq_.process(recombined,eq_freq,eq_gain);

        // 8. Stereo Ping-Pong Delay
        float delayed_l = 0.0f;
        float delayed_r = 0.0f;
        delay_.process(equalized, equalized, delayed_l, delayed_r);

        // 9. FDN Algorithmic Reverb
        float reverbed_l = 0.0f;
        float reverbed_r = 0.0f;
        fdn_reverb_.process(delayed_l, delayed_r, reverbed_l, reverbed_r);

        // 10. Master Gain & Zero-Latency Safety Limiter
        float out_l = reverbed_l * master_volume_;
        float out_r = reverbed_r * master_volume_;
        safety_limiter_.process(out_l, out_r);

        output_l[i] = out_l;
        output_r[i] = out_r;
    }

    latency_meter_.end_block();
}

LatencyStats MonkeysEarEngine::get_latency_stats() const {
    return latency_meter_.get_stats();
}

} // namespace monkeys_ear
