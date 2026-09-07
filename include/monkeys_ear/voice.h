#pragma once

#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

enum class Waveform {
    Saw = 0,
    Pulse,
    Triangle,
    Sine
};

enum class EnvStage {
    Idle = 0,
    Attack,
    Decay,
    Sustain,
    Release
};

class Envelope {
public:
    Envelope();
    void set_sample_rate(float sr);
    void set_parameters(float attack_s, float decay_s, float sustain_lvl, float release_s);
    void trigger(float velocity);
    void release();
    void reset();
    float process();
    bool is_idle() const { return stage_ == EnvStage::Idle; }
    float get_value() const { return current_value_; }

private:
    float sample_rate_;
    float attack_time_;
    float decay_time_;
    float sustain_level_;
    float release_time_;

    EnvStage stage_;
    float current_value_;
    float attack_increment_;
    float decay_coeff_;
    float release_coeff_;
    float velocity_;

    void recalculate_rates();
};

class PolyBLEPOscillator {
public:
    PolyBLEPOscillator();
    void set_sample_rate(float sr);
    void set_frequency(float freq);
    void set_pulse_width(float pw);
    void set_waveform(Waveform wf);
    void reset_phase();
    void set_phase(float phase_cycles);
    void sync_reset();
    bool has_wrapped() const { return wrapped_; }

    float process();
    float process_with_pm(float phase_mod);

private:
    float sample_rate_;
    float frequency_;
    float phase_;
    float phase_increment_;
    float pulse_width_;
    Waveform waveform_;
    bool wrapped_;

    float poly_blep(float t, float dt) const;
    void update_increment();
};

class SynthVoice {
public:
    SynthVoice();
    void set_sample_rate(float sr);
    void note_on(int note, float velocity);
    void note_off();
    void set_pitch_bend(float semitones);
    void set_waveform(Waveform wf);
    void set_sub_mix(float sub_mix);
    void set_fundamental_mix(float mix);
    void set_sub_ratio(int denominator);
    void set_sub_phase(float phase_cycles);
    void set_sub_polarity(bool inverted);
    void set_sub_saturation(float amount);
    void set_sub_envelope(float amount);
    void set_noise_mix(float noise_mix);
    void set_detune(float detune_cents);
    void set_fm_amount(float fm);
    void set_osc2_semi(int semi);
    void set_hard_sync(bool sync);
    void set_env_parameters(float a, float d, float s, float r);
    void set_filter_env_parameters(float a, float d, float s, float r);
    void set_portamento(float seconds);
    void set_vibrato(float semitones);
    void retarget_note(int note, float velocity, bool retrigger);

    float process(float& filter_env_out);
    float process_split(float& filter_env_out, float& weight_out);
    bool is_active() const;
    int get_note() const { return note_; }
    float get_age() const { return age_; }

private:
    float sample_rate_;
    int note_;
    float velocity_;
    float base_frequency_;
    float pitch_bend_semitones_;
    float detune_cents_;
    int osc2_semi_;
    float fm_amount_;
    bool hard_sync_;
    float sub_mix_;
    float fundamental_mix_;
    int sub_ratio_denominator_;
    float sub_polarity_;
    float sub_saturation_;
    float sub_envelope_amount_;
    float noise_mix_;
    float age_;

    PolyBLEPOscillator osc1_;
    PolyBLEPOscillator osc2_;
    PolyBLEPOscillator sub_osc_;
    PolyBLEPOscillator fundamental_osc_;
    Envelope amp_env_;
    Envelope filter_env_;

    uint32_t noise_seed_;
    float current_frequency_;
    float target_frequency_;
    float portamento_seconds_;
    float vibrato_semitones_;
    float next_noise();
    void update_frequencies();
};

class VoiceManager {
public:
    static constexpr size_t MAX_VOICES = 16;

    VoiceManager();
    void set_sample_rate(float sr);
    void note_on(int note, float velocity);
    void note_off(int note);
    void all_notes_off();
    void set_pitch_bend(float semitones);
    void set_waveform(Waveform wf);
    void set_sub_mix(float sub_mix);
    void set_fundamental_mix(float mix);
    void set_sub_ratio(int denominator);
    void set_sub_phase(float phase_cycles);
    void set_sub_polarity(bool inverted);
    void set_sub_saturation(float amount);
    void set_sub_envelope(float amount);
    void set_noise_mix(float noise_mix);
    void set_detune(float detune_cents);
    void set_fm_amount(float fm);
    void set_osc2_semi(int semi);
    void set_hard_sync(bool sync);
    void set_amp_envelope(float a, float d, float s, float r);
    void set_filter_envelope(float a, float d, float s, float r);
    void set_voice_mode(bool mono, bool legato);
    void set_portamento(float seconds);
    void set_vibrato(float semitones);

    float process(float& out_filter_env);
    float process_split(float& out_filter_env, float& out_weight);
    size_t active_voice_count() const;

private:
    float sample_rate_;
    std::array<SynthVoice, MAX_VOICES> voices_;
    float pitch_bend_;
    float fm_amount_;
    int osc2_semi_;
    bool hard_sync_;
    bool mono_;
    bool legato_;

    int find_free_voice();
};

} // namespace monkeys_ear
