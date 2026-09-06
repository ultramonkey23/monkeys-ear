#pragma once

#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

enum class Waveform {
    Saw,
    Pulse,
    Triangle,
    Sine
};

enum class EnvStage {
    Idle,
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
    float process();

private:
    float sample_rate_;
    float frequency_;
    float phase_;
    float phase_increment_;
    float pulse_width_;
    Waveform waveform_;

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
    void set_noise_mix(float noise_mix);
    void set_detune(float detune_cents);
    void set_env_parameters(float a, float d, float s, float r);
    void set_filter_env_parameters(float a, float d, float s, float r);

    float process(float& filter_env_out);
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
    float sub_mix_;
    float noise_mix_;
    float age_;

    PolyBLEPOscillator osc1_;
    PolyBLEPOscillator osc2_;
    PolyBLEPOscillator sub_osc_;
    Envelope amp_env_;
    Envelope filter_env_;

    uint32_t noise_seed_;
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
    void set_noise_mix(float noise_mix);
    void set_detune(float detune_cents);
    void set_amp_envelope(float a, float d, float s, float r);
    void set_filter_envelope(float a, float d, float s, float r);

    float process(float& out_filter_env);
    size_t active_voice_count() const;

private:
    float sample_rate_;
    std::array<SynthVoice, MAX_VOICES> voices_;
    float pitch_bend_;

    int find_free_voice();
};

} // namespace monkeys_ear
