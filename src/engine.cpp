#include "monkeys_ear/engine.h"
#include <cmath>

namespace monkeys_ear {

MonkeysEarEngine::MonkeysEarEngine()
    : sample_rate_(48000.0f),
      max_block_size_(512),
      master_volume_(0.85f) {
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
    filter_.set_cutoff(cutoff_hz);

    // Macro 2: Bite / Filter Resonance (0.05 to 0.92)
    float m_res = p.macros[MACRO_RESONANCE];
    filter_.set_resonance(0.05f + m_res * 0.85f);

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

    // Macro 8: Dynamic Character / Cathode Sag
    float m_char = p.macros[MACRO_CHARACTER];
    drive_tube_.set_memory_sag(m_char);

    // Master volume from preset
    master_volume_ = std::pow(10.0f, p.master_gain_db / 20.0f) * 0.85f;

    // Advanced Sound Construction: Oscillators
    voice_manager_.set_waveform(static_cast<Waveform>(p.synth_waveform));
    voice_manager_.set_sub_mix(p.sub_mix);
    voice_manager_.set_noise_mix(p.noise_mix);
    voice_manager_.set_fm_amount(p.osc_fm_amount);
    voice_manager_.set_osc2_semi(p.osc2_semi);
    voice_manager_.set_hard_sync(p.osc_hard_sync);
    voice_manager_.set_amp_envelope(p.amp_attack, p.amp_decay, p.amp_sustain, p.amp_release);
    voice_manager_.set_filter_envelope(p.filter_attack, p.filter_decay, p.filter_sustain, p.filter_release);

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
    auto p = preset_manager_.get_current();
    p.master_gain_db = db;
    load_preset(p);
}

void MonkeysEarEngine::set_waveform(Waveform wf) {
    auto p = preset_manager_.get_current();
    p.synth_waveform = static_cast<int>(wf);
    load_preset(p);
}

void MonkeysEarEngine::set_osc_fm(float fm) {
    auto p = preset_manager_.get_current();
    p.osc_fm_amount = fm;
    load_preset(p);
}

void MonkeysEarEngine::set_osc2_semi(int semi) {
    auto p = preset_manager_.get_current();
    p.osc2_semi = semi;
    load_preset(p);
}

void MonkeysEarEngine::set_osc_hard_sync(bool sync) {
    auto p = preset_manager_.get_current();
    p.osc_hard_sync = sync;
    load_preset(p);
}

void MonkeysEarEngine::set_state_resistance(float r) {
    auto p = preset_manager_.get_current();
    p.state_resistance = r;
    load_preset(p);
}

void MonkeysEarEngine::set_state_repulsion(float k) {
    auto p = preset_manager_.get_current();
    p.state_repulsion = k;
    load_preset(p);
}

void MonkeysEarEngine::set_state_coupling(float kappa) {
    auto p = preset_manager_.get_current();
    p.state_coupling = kappa;
    load_preset(p);
}

void MonkeysEarEngine::set_state_persistence(float tau) {
    auto p = preset_manager_.get_current();
    p.state_persistence = tau;
    load_preset(p);
}

void MonkeysEarEngine::set_state_enabled(bool en) {
    auto p = preset_manager_.get_current();
    p.state_enabled = en;
    load_preset(p);
}

void MonkeysEarEngine::set_lfo1_rate(float hz) {
    auto p = preset_manager_.get_current();
    p.lfo1_rate_hz = hz;
    load_preset(p);
}

void MonkeysEarEngine::set_lfo1_depth(float depth) {
    auto p = preset_manager_.get_current();
    p.lfo1_depth = depth;
    load_preset(p);
}

void MonkeysEarEngine::set_input_route_mode(int mode) {
    auto p = preset_manager_.get_current();
    p.input_route_mode = mode;
    load_preset(p);
}

void MonkeysEarEngine::handle_midi_note_on(int note, float velocity) {
    voice_manager_.note_on(note, velocity);
}

void MonkeysEarEngine::handle_midi_note_off(int note) {
    voice_manager_.note_off(note);
}

void MonkeysEarEngine::handle_midi_pitch_bend(float semitones) {
    voice_manager_.set_pitch_bend(semitones);
}

void MonkeysEarEngine::handle_midi_cc(int cc_number, float value_0_to_1) {
    switch (cc_number) {
        case 1:  set_macro(MACRO_CUTOFF, value_0_to_1); break;
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
        float lfo_val = lfo1_.process();

        // 1. Synth Voices Generation
        float filter_env = 0.0f;
        float synth_sample = voice_manager_.process(filter_env);

        // 2. Live External Audio Ingest & Preamp/Conditioning
        float ext_sample = 0.0f;
        if (input_l != nullptr) {
            ext_sample = (input_r != nullptr) ? 0.5f * (input_l[i] + input_r[i]) : input_l[i];
        }
        float blended_ext = audio_input_.process_sample(ext_sample, synth_sample);

        // 3. Routing Mode Resolution
        float exciter = 0.0f;
        if (p.input_route_mode == 2) {
            // Pure External Audio Processor (Guitar / Mic / Recorded Audio)
            exciter = (input_l != nullptr) ? ext_sample : 0.0f;
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

        // 5. State Variable Filter (with envelope and LFO modulation)
        float mod_env = filter_env + lfo_val * 0.25f;
        float filtered = filter_.process(stateful_source, mod_env, env_amt);

        // 6. Tube Drive Stage (with dynamic cathode sag memory)
        float driven = drive_tube_.process(filtered);

        // 7. Modal Cabinet Resonator
        float resonated = resonator_cab_.process(driven);

        // 8. Stereo Ping-Pong Delay
        float delayed_l = 0.0f;
        float delayed_r = 0.0f;
        delay_.process(resonated, resonated, delayed_l, delayed_r);

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
