#pragma once

#include "monkeys_ear/types.h"
#include "monkeys_ear/mono_key_stack.h"
#include "monkeys_ear/wavetable.h"
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
    void set_wave_families(WaveFamily a, WaveFamily b);
    void set_morph(float morph01);
    void set_phase_warp(float warp01);
    void set_wave_fold(float fold01);
    void set_unison(int voices, float detune_cents);
    void set_waveform_bank(const WaveformBank* bank);
    void reset_phase();
    void set_phase(float phase_cycles);
    void sync_reset();
    void sync_reset_subsample(float master_wrap_frac);
    bool has_wrapped() const { return wrapped_; }
    float wrap_fraction() const { return wrap_fraction_; }

    float process();
    float process_with_pm(float phase_mod);
    float process_with_pm(float phase_mod, float morph_mod);

private:
    static constexpr size_t kMaxUnison = 5;
    float sample_rate_;
    float frequency_;
    float phase_;
    float phase_increment_;
    float pulse_width_;
    Waveform waveform_;
    WaveFamily family_a_ = WaveFamily::Saw;
    WaveFamily family_b_ = WaveFamily::Formant;
    float morph_ = 0.0f;
    float phase_warp_ = 0.5f;
    float wave_fold_ = 0.0f;
    int unison_voices_ = 1;
    float unison_detune_cents_ = 12.0f;
    const WaveformBank* bank_ = nullptr;
    std::array<float, kMaxUnison> unison_phases_{{0.0f, 0.21f, 0.47f, 0.68f, 0.84f}};
    bool wrapped_;
    float wrap_fraction_ = 0.0f;
    float last_sample_ = 0.0f;
    float sync_blep_pending_ = 0.0f;

    float poly_blep(float t, float dt) const;
    float eval_single_phase(float p, float dt, float morph01) const;
    void update_increment();
};

class SynthVoice {
public:
    SynthVoice();
    void set_sample_rate(float sr);
    void note_on(int note, float velocity);
    void note_off();
    void kill(); // immediate voice reset on transport/host deactivation
    void set_pitch_bend(float semitones);
    void set_poly_pressure(float value); // individual voice timbre articulation
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
    void set_wave_morph_controls(int family_a, int family_b, float morph,
                                 float phase_warp, float wave_fold,
                                 int unison_voices, float unison_detune_cents,
                                 float sub_harmonics, float env_to_morph);
    void set_lfo_morph_mod(float amount);
    void set_waveform_bank(const WaveformBank* bank);
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
    float poly_pressure_=0.0f;
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
    float sub_harmonics_ = 0.0f;
    float env_to_morph_ = 0.0f;
    float lfo_morph_mod_ = 0.0f;
    float next_noise();
    void update_frequencies();
};

class VoiceManager {
public:
    static constexpr size_t MAX_VOICES = 16;

    VoiceManager();
    void set_sample_rate(float sr);
    void note_on(int note,float velocity,int channel=0,int note_id=-1);
    // Each key-off releases exactly one matching held voice.
    void note_off(int note,int channel=-1,int note_id=-1);
    void set_poly_pressure(int note,int channel,int note_id,float value);
    void all_notes_off();
    void all_sounds_off();
    void set_sustain_pedal(bool down);
    bool sustain_pedal_down() const { return sustain_down_; }
    int current_mono_note() const {
        return mono_&&voices_[0].is_active()?voices_[0].get_note():-1;
    }
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
    void set_wave_morph_controls(int family_a, int family_b, float morph,
                                 float phase_warp, float wave_fold,
                                 int unison_voices, float unison_detune_cents,
                                 float sub_harmonics, float env_to_morph);
    void set_lfo_morph_mod(float amount);
    void set_generator_params(const WaveformGeneratorParams& params);
    const WaveformBank& waveform_bank() const { return bank_; }
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
    bool sustain_down_ = false;
    std::array<bool, MAX_VOICES> deferred_release_{};
    std::array<bool, MAX_VOICES> key_held_{};
    std::array<int, MAX_VOICES> voice_channel_{};
    std::array<int, MAX_VOICES> voice_note_id_{};
    std::array<uint32_t, MAX_VOICES> key_generation_{};
    uint32_t next_key_generation_=1;

    MonoKeyStack mono_keys_{}; // fixed-capacity last-held-key priority
    WaveformBank bank_{};

    int find_free_voice();
};

} // namespace monkeys_ear
