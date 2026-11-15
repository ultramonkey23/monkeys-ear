#include "monkeys_ear/voice.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear {

// ── Envelope Implementation ──────────────────────────────────────────
Envelope::Envelope()
    : sample_rate_(48000.0f),
      attack_time_(0.01f),
      decay_time_(0.1f),
      sustain_level_(0.7f),
      release_time_(0.2f),
      stage_(EnvStage::Idle),
      current_value_(0.0f),
      attack_increment_(0.0f),
      decay_coeff_(0.0f),
      release_coeff_(0.0f),
      velocity_(1.0f) {
    recalculate_rates();
}

void Envelope::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    recalculate_rates();
}

void Envelope::set_parameters(float attack_s, float decay_s, float sustain_lvl, float release_s) {
    attack_time_ = std::max(0.001f, attack_s);
    decay_time_ = std::max(0.001f, decay_s);
    sustain_level_ = clamp(sustain_lvl, 0.0f, 1.0f);
    release_time_ = std::max(0.001f, release_s);
    recalculate_rates();
}

void Envelope::recalculate_rates() {
    float attack_samples = attack_time_ * sample_rate_;
    attack_increment_ = 1.0f / attack_samples;
    decay_coeff_ = std::exp(-1.0f / (decay_time_ * sample_rate_ * 0.3f));
    release_coeff_ = std::exp(-1.0f / (release_time_ * sample_rate_ * 0.3f));
}

void Envelope::trigger(float velocity) {
    velocity_ = clamp(velocity, 0.0f, 1.0f);
    stage_ = EnvStage::Attack;
}

void Envelope::release() {
    if (stage_ != EnvStage::Idle) {
        stage_ = EnvStage::Release;
    }
}

void Envelope::reset() {
    stage_ = EnvStage::Idle;
    current_value_ = 0.0f;
}

float Envelope::process() {
    switch (stage_) {
        case EnvStage::Idle:
            current_value_ = 0.0f;
            break;

        case EnvStage::Attack:
            current_value_ += attack_increment_;
            if (current_value_ >= 1.0f) {
                current_value_ = 1.0f;
                stage_ = EnvStage::Decay;
            }
            break;

        case EnvStage::Decay:
            current_value_ = sustain_level_ + (current_value_ - sustain_level_) * decay_coeff_;
            if (std::abs(current_value_ - sustain_level_) < 1e-4f) {
                current_value_ = sustain_level_;
                stage_ = EnvStage::Sustain;
            }
            break;

        case EnvStage::Sustain:
            current_value_ = sustain_level_;
            break;

        case EnvStage::Release:
            current_value_ *= release_coeff_;
            if (current_value_ < 1e-4f) {
                current_value_ = 0.0f;
                stage_ = EnvStage::Idle;
            }
            break;
    }
    return current_value_ * velocity_;
}

// ── PolyBLEPOscillator Implementation ──────────────────────────────────
PolyBLEPOscillator::PolyBLEPOscillator()
    : sample_rate_(48000.0f),
      frequency_(440.0f),
      phase_(0.0f),
      phase_increment_(0.0f),
      pulse_width_(0.5f),
      waveform_(Waveform::Saw),
      wrapped_(false) {
    update_increment();
}

void PolyBLEPOscillator::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    update_increment();
}

void PolyBLEPOscillator::set_frequency(float freq) {
    frequency_ = clamp(freq, 10.0f, sample_rate_ * 0.49f);
    update_increment();
}

void PolyBLEPOscillator::set_pulse_width(float pw) {
    pulse_width_ = clamp(pw, 0.05f, 0.95f);
}

void PolyBLEPOscillator::set_waveform(Waveform wf) {
    waveform_ = wf;
    switch (wf) {
        case Waveform::Saw: family_a_ = WaveFamily::Saw; break;
        case Waveform::Pulse: family_a_ = WaveFamily::Pulse; break;
        case Waveform::Triangle: family_a_ = WaveFamily::Triangle; break;
        case Waveform::Sine: family_a_ = WaveFamily::Sine; break;
    }
}

void PolyBLEPOscillator::set_wave_families(WaveFamily a, WaveFamily b) {
    family_a_ = a;
    family_b_ = b;
}

void PolyBLEPOscillator::set_morph(float morph01) {
    morph_ = clamp(morph01, 0.0f, 1.0f);
}

void PolyBLEPOscillator::set_phase_warp(float warp01) {
    phase_warp_ = clamp(warp01, 0.05f, 0.95f);
    pulse_width_ = phase_warp_;
}

void PolyBLEPOscillator::set_wave_fold(float fold01) {
    wave_fold_ = clamp(fold01, 0.0f, 1.0f);
}

void PolyBLEPOscillator::set_unison(int voices, float detune_cents) {
    unison_voices_ = std::max(1, std::min(static_cast<int>(kMaxUnison), voices));
    unison_detune_cents_ = clamp(detune_cents, 0.0f, 50.0f);
}

void PolyBLEPOscillator::set_waveform_bank(const WaveformBank* bank) {
    bank_ = bank;
}

void PolyBLEPOscillator::reset_phase() {
    phase_ = 0.0f;
    unison_phases_ = {{0.0f, 0.21f, 0.47f, 0.68f, 0.84f}};
    wrapped_ = false;
    wrap_fraction_ = 0.0f;
    last_sample_ = 0.0f;
    sync_blep_pending_ = 0.0f;
}

void PolyBLEPOscillator::set_phase(float phase_cycles) {
    phase_ = phase_cycles - std::floor(phase_cycles);
    if (phase_ < 0.0f) phase_ += 1.0f;
    wrapped_ = false;
    wrap_fraction_ = 0.0f;
}

void PolyBLEPOscillator::sync_reset() {
    sync_reset_subsample(0.0f);
}

void PolyBLEPOscillator::sync_reset_subsample(float master_wrap_frac) {
    const float frac = clamp(master_wrap_frac, 0.0f, 1.0f);
    const float old_val = eval_single_phase(phase_, phase_increment_, morph_);
    const float new_phase = frac * phase_increment_;
    const float new_val = eval_single_phase(new_phase, phase_increment_, morph_);
    // PolyBLEP discontinuity step compensation across hard-sync transition
    sync_blep_pending_ = (old_val - new_val) * 0.5f;
    phase_ = new_phase - std::floor(new_phase);
    for (size_t u = 1; u < kMaxUnison; ++u) {
        unison_phases_[u] = std::fmod(phase_ + 0.19f * static_cast<float>(u), 1.0f);
    }
    wrapped_ = false;
}

void PolyBLEPOscillator::update_increment() {
    phase_increment_ = frequency_ / sample_rate_;
}

float PolyBLEPOscillator::poly_blep(float t, float dt) const {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float PolyBLEPOscillator::eval_single_phase(float p, float dt, float eff_morph) const {
    float eff_phase = p - std::floor(p);
    if (eff_phase < 0.0f) eff_phase += 1.0f;

    const bool use_legacy_core =
        (static_cast<int>(family_a_) <= 3) &&
        (eff_morph <= 1e-4f) &&
        (wave_fold_ <= 1e-4f) &&
        (std::abs(phase_warp_ - 0.5f) <= 1e-4f || family_a_ == WaveFamily::Pulse);

    if (use_legacy_core) {
        switch (family_a_) {
            case WaveFamily::Sine:
                return std::sin(eff_phase * TWO_PI);
            case WaveFamily::Saw: {
                float s = (2.0f * eff_phase) - 1.0f;
                s -= poly_blep(eff_phase, dt);
                return s;
            }
            case WaveFamily::Pulse: {
                float s = (eff_phase < pulse_width_) ? 1.0f : -1.0f;
                s += poly_blep(eff_phase, dt);
                float p2 = eff_phase - pulse_width_;
                if (p2 < 0.0f) p2 += 1.0f;
                s -= poly_blep(p2, dt);
                return s;
            }
            case WaveFamily::Triangle: {
                float saw = (2.0f * eff_phase) - 1.0f - poly_blep(eff_phase, dt);
                return 2.0f * std::abs(saw) - 1.0f;
            }
            default:
                break;
        }
    }

    const WaveformBank& active_bank = bank_ ? *bank_ : shared_waveform_bank();
    return active_bank.sample_morph(family_a_, family_b_, eff_morph, eff_phase, dt, phase_warp_, wave_fold_);
}

float PolyBLEPOscillator::process() {
    return process_with_pm(0.0f, 0.0f);
}

float PolyBLEPOscillator::process_with_pm(float phase_mod) {
    return process_with_pm(phase_mod, 0.0f);
}

float PolyBLEPOscillator::process_with_pm(float phase_mod, float morph_mod) {
    const float dt = phase_increment_;
    const float eff_morph = clamp(morph_ + morph_mod, 0.0f, 1.0f);
    float out = 0.0f;

    if (unison_voices_ <= 1) {
        out = eval_single_phase(phase_ + phase_mod, dt, eff_morph);
        if (std::abs(sync_blep_pending_) > 1e-5f) {
            out += sync_blep_pending_ * poly_blep(phase_, std::max(1e-5f, dt));
            sync_blep_pending_ = 0.0f;
        }
    } else {
        const float half = 0.5f * static_cast<float>(unison_voices_ - 1);
        float sum = eval_single_phase(phase_ + phase_mod, dt, eff_morph);
        for (int u = 1; u < unison_voices_; ++u) {
            const float offset_index = static_cast<float>(u) - half;
            const float detune_ratio = std::pow(2.0f, (offset_index * unison_detune_cents_) / (1200.0f * std::max(1.0f, half)));
            const float u_dt = dt * detune_ratio;
            sum += eval_single_phase(unison_phases_[static_cast<size_t>(u)] + phase_mod, u_dt, eff_morph);
            unison_phases_[static_cast<size_t>(u)] += u_dt;
            if (unison_phases_[static_cast<size_t>(u)] >= 1.0f) {
                unison_phases_[static_cast<size_t>(u)] -= std::floor(unison_phases_[static_cast<size_t>(u)]);
            }
        }
        out = sum / std::sqrt(static_cast<float>(unison_voices_));
    }

    last_sample_ = out;
    phase_ += phase_increment_;
    if (phase_ >= 1.0f) {
        wrap_fraction_ = clamp((phase_ - 1.0f) / std::max(1e-6f, phase_increment_), 0.0f, 1.0f);
        phase_ -= std::floor(phase_);
        wrapped_ = true;
    } else {
        wrap_fraction_ = 0.0f;
        wrapped_ = false;
    }

    return sanitize(out);
}

// ── SynthVoice Implementation ──────────────────────────────────────────
SynthVoice::SynthVoice()
    : sample_rate_(48000.0f),
      note_(-1),
      velocity_(0.0f),
      base_frequency_(440.0f),
      pitch_bend_semitones_(0.0f),
      detune_cents_(7.0f),
      osc2_semi_(0),
      fm_amount_(0.0f),
      hard_sync_(false),
      sub_mix_(0.25f),
      fundamental_mix_(0.20f),
      sub_ratio_denominator_(2),
      sub_polarity_(1.0f),
      sub_saturation_(0.0f),
      sub_envelope_amount_(1.0f),
      noise_mix_(0.02f),
      age_(0.0f),
      noise_seed_(0x12345678),
      current_frequency_(440.0f),
      target_frequency_(440.0f),
      portamento_seconds_(0.0f),
      vibrato_semitones_(0.0f) {
    osc1_.set_waveform(Waveform::Saw);
    osc2_.set_waveform(Waveform::Pulse);
    sub_osc_.set_waveform(Waveform::Sine);
    fundamental_osc_.set_waveform(Waveform::Sine);
}

void SynthVoice::set_sample_rate(float sr) {
    sample_rate_ = sr;
    osc1_.set_sample_rate(sr);
    osc2_.set_sample_rate(sr);
    sub_osc_.set_sample_rate(sr);
    fundamental_osc_.set_sample_rate(sr);
    amp_env_.set_sample_rate(sr);
    filter_env_.set_sample_rate(sr);
}

void SynthVoice::note_on(int note, float velocity) {
    note_ = note;
    velocity_ = velocity;
    poly_pressure_=0.0f;
    base_frequency_ = midi_to_freq(static_cast<float>(note));
    target_frequency_ = base_frequency_;
    if (!is_active() || portamento_seconds_ <= 0.0001f) current_frequency_ = target_frequency_;
    age_ = 0.0f;

    update_frequencies();
    osc1_.reset_phase();
    osc2_.reset_phase();
    sub_osc_.reset_phase();
    fundamental_osc_.reset_phase();

    amp_env_.trigger(velocity);
    filter_env_.trigger(velocity);
}

void SynthVoice::retarget_note(int note, float velocity, bool retrigger) {
    note_ = note;
    velocity_ = velocity;
    poly_pressure_=0.0f;
    base_frequency_ = midi_to_freq(static_cast<float>(note));
    target_frequency_ = base_frequency_;
    if (retrigger) {
        amp_env_.trigger(velocity);
        filter_env_.trigger(velocity);
    }
}

void SynthVoice::note_off() {
    amp_env_.release();
    filter_env_.release();
}

void SynthVoice::kill() {
    amp_env_.reset();
    filter_env_.reset();
    poly_pressure_=0.0f;
}

void SynthVoice::set_pitch_bend(float semitones) {
    pitch_bend_semitones_ = semitones;
    update_frequencies();
}

void SynthVoice::set_poly_pressure(float value) {
    poly_pressure_=clamp(value,0.0f,1.0f);
}

void SynthVoice::set_waveform(Waveform wf) {
    osc1_.set_waveform(wf);
    osc2_.set_waveform(wf);
}

void SynthVoice::set_sub_mix(float sub_mix) {
    sub_mix_ = clamp(sub_mix, 0.0f, 1.0f);
}

void SynthVoice::set_fundamental_mix(float mix) { fundamental_mix_ = clamp(mix, 0.0f, 1.0f); }
void SynthVoice::set_sub_ratio(int denominator) { sub_ratio_denominator_ = clamp(denominator, 1, 4); update_frequencies(); }
void SynthVoice::set_sub_phase(float phase_cycles) { sub_osc_.set_phase(phase_cycles); }
void SynthVoice::set_sub_polarity(bool inverted) { sub_polarity_ = inverted ? -1.0f : 1.0f; }
void SynthVoice::set_sub_saturation(float amount) { sub_saturation_ = clamp(amount, 0.0f, 1.0f); }
void SynthVoice::set_sub_envelope(float amount) { sub_envelope_amount_ = clamp(amount, 0.0f, 1.0f); }
void SynthVoice::set_portamento(float seconds) { portamento_seconds_ = clamp(seconds, 0.0f, 2.0f); }
void SynthVoice::set_vibrato(float semitones) { vibrato_semitones_ = clamp(semitones, -2.0f, 2.0f); }

void SynthVoice::set_noise_mix(float noise_mix) {
    noise_mix_ = clamp(noise_mix, 0.0f, 1.0f);
}

void SynthVoice::set_detune(float detune_cents) {
    detune_cents_ = detune_cents;
    update_frequencies();
}

void SynthVoice::set_fm_amount(float fm) {
    fm_amount_ = clamp(fm, 0.0f, 1.0f);
}

void SynthVoice::set_osc2_semi(int semi) {
    osc2_semi_ = clamp(semi, -24, 24);
    update_frequencies();
}

void SynthVoice::set_hard_sync(bool sync) {
    hard_sync_ = sync;
}

void SynthVoice::set_wave_morph_controls(int family_a, int family_b, float morph,
                                         float phase_warp, float wave_fold,
                                         int unison_voices, float unison_detune_cents,
                                         float sub_harmonics, float env_to_morph) {
    const WaveFamily fa = static_cast<WaveFamily>(clamp(family_a, 0, kWaveFamilyCount - 1));
    const WaveFamily fb = static_cast<WaveFamily>(clamp(family_b, 0, kWaveFamilyCount - 1));
    osc1_.set_wave_families(fa, fb);
    osc2_.set_wave_families(fa, fb);
    osc1_.set_morph(morph);
    osc2_.set_morph(morph);
    osc1_.set_phase_warp(phase_warp);
    osc2_.set_phase_warp(phase_warp);
    osc1_.set_wave_fold(wave_fold);
    osc2_.set_wave_fold(wave_fold);
    osc1_.set_unison(unison_voices, unison_detune_cents);

    sub_harmonics_ = clamp(sub_harmonics, 0.0f, 1.0f);
    sub_osc_.set_wave_families(WaveFamily::Sine, WaveFamily::BrassFold);
    sub_osc_.set_morph(sub_harmonics_);
    env_to_morph_ = clamp(env_to_morph, -1.0f, 1.0f);
}

void SynthVoice::set_lfo_morph_mod(float amount) {
    lfo_morph_mod_ = clamp(amount, -1.0f, 1.0f);
}

void SynthVoice::set_waveform_bank(const WaveformBank* bank) {
    osc1_.set_waveform_bank(bank);
    osc2_.set_waveform_bank(bank);
    sub_osc_.set_waveform_bank(bank);
}

void SynthVoice::set_env_parameters(float a, float d, float s, float r) {
    amp_env_.set_parameters(a, d, s, r);
}

void SynthVoice::set_filter_env_parameters(float a, float d, float s, float r) {
    filter_env_.set_parameters(a, d, s, r);
}

void SynthVoice::update_frequencies() {
    float bend_factor = std::pow(2.0f, pitch_bend_semitones_ / 12.0f);
    target_frequency_ = base_frequency_ * bend_factor;
    if (portamento_seconds_ <= 0.0001f) current_frequency_ = target_frequency_;
    float f1 = current_frequency_ * std::pow(2.0f, vibrato_semitones_ / 12.0f);
    float f2_ratio = std::pow(2.0f, (static_cast<float>(osc2_semi_) + detune_cents_ / 100.0f) / 12.0f);
    float f2 = f1 * f2_ratio;
    float f_sub = f1 / static_cast<float>(sub_ratio_denominator_);

    osc1_.set_frequency(f1);
    osc2_.set_frequency(f2);
    sub_osc_.set_frequency(f_sub);
    fundamental_osc_.set_frequency(f1);
}

float SynthVoice::next_noise() {
    noise_seed_ = noise_seed_ * 1664525u + 1013904223u;
    return (static_cast<float>(noise_seed_) / 4294967296.0f) * 2.0f - 1.0f;
}

bool SynthVoice::is_active() const {
    return !amp_env_.is_idle();
}

float SynthVoice::process(float& filter_env_out) {
    float weight = 0.0f;
    return sanitize(process_split(filter_env_out, weight) + weight);
}

float SynthVoice::process_split(float& filter_env_out, float& weight_out) {
    if (!is_active()) {
        filter_env_out = 0.0f;
        weight_out = 0.0f;
        return 0.0f;
    }

    age_ += 1.0f / sample_rate_;
    if (portamento_seconds_ > 0.0001f) {
        float glide = 1.0f - std::exp(-1.0f / (portamento_seconds_ * sample_rate_));
        current_frequency_ += (target_frequency_ - current_frequency_) * glide;
    } else current_frequency_ = target_frequency_;
    float f1 = current_frequency_ * std::pow(2.0f, vibrato_semitones_ / 12.0f);
    osc1_.set_frequency(f1);
    osc2_.set_frequency(f1 * std::pow(2.0f, (static_cast<float>(osc2_semi_) + detune_cents_/100.0f)/12.0f));
    sub_osc_.set_frequency(f1 / static_cast<float>(sub_ratio_denominator_));
    fundamental_osc_.set_frequency(f1);

    // Modulator Oscillator (Osc 2) also tracks dynamic morph modulation
    float env_morph = env_to_morph_ * filter_env_.get_value();
    float s2 = osc2_.process_with_pm(0.0f, env_morph + lfo_morph_mod_);

    // Hard sync: sub-sample phase reset with PolyBLEP discontinuity compensation
    if (hard_sync_ && osc2_.has_wrapped()) {
        osc1_.sync_reset_subsample(osc2_.wrap_fraction());
    }

    // Carrier Oscillator (Osc 1) with Cross-FM Phase Modulation + Dynamic Morph Modulation
    float pm_input = (fm_amount_ > 0.001f) ? (s2 * fm_amount_ * 3.5f) : 0.0f;
    float s1 = osc1_.process_with_pm(pm_input, env_morph + lfo_morph_mod_);

    float s_sub = sub_osc_.process();
    float s_fundamental = fundamental_osc_.process();
    float noise = next_noise();

    // Sum interacting sources
    float carrier_level = 0.55f;
    float mod_level = 0.45f * (1.0f - fm_amount_ * 0.35f)
        * (1.0f + 0.45f * poly_pressure_); // per-key expressive harmonic emphasis
    float character = carrier_level * s1 + mod_level * s2 + noise_mix_ * noise;

    float amp = amp_env_.process();
    filter_env_out = filter_env_.process();
    float sub_env = lerp(1.0f, amp, sub_envelope_amount_);
    float sub = s_sub * sub_polarity_;
    if (sub_saturation_ > 0.0f) sub = lerp(sub, fast_tanh(sub * (1.0f + 4.0f*sub_saturation_)), sub_saturation_);
    weight_out = sanitize((fundamental_mix_ * s_fundamental + sub_mix_ * sub) * sub_env);
    return sanitize(character * amp);
}

// ── VoiceManager Implementation ────────────────────────────────────────
VoiceManager::VoiceManager()
    : sample_rate_(48000.0f),
      pitch_bend_(0.0f),
      fm_amount_(0.0f),
      osc2_semi_(0),
      hard_sync_(false),
      mono_(false),
      legato_(false) {
}

void VoiceManager::set_sample_rate(float sr) {
    sample_rate_ = sr;
    for (auto& v : voices_) {
        v.set_sample_rate(sr);
    }
}

int VoiceManager::find_free_voice() {
    for (size_t i = 0; i < MAX_VOICES; ++i) {
        if (!voices_[i].is_active()) {
            return static_cast<int>(i);
        }
    }
    int oldest_idx = 0;
    float max_age = -1.0f;
    for (size_t i = 0; i < MAX_VOICES; ++i) {
        if (voices_[i].get_age() > max_age) {
            max_age = voices_[i].get_age();
            oldest_idx = static_cast<int>(i);
        }
    }
    return oldest_idx;
}

void VoiceManager::note_on(int note,float velocity,int channel,int note_id) {
    if(note<0||note>127||channel<0||channel>15)return;
    const int idx=mono_?0:find_free_voice();
    if(idx<0||idx>=static_cast<int>(MAX_VOICES))return;
    const size_t slot=static_cast<size_t>(idx);
    if(mono_){
        // Record every held key, not merely the sounding key. On top-key
        // release, legato must return to the earlier key without a new note-on.
        mono_keys_.push({note,channel,note_id,velocity});
        if(voices_[0].is_active())
            voices_[0].retarget_note(note,velocity,!legato_);
        else voices_[0].note_on(note,velocity);
    }else{
        voices_[slot].note_on(note,velocity);
    }
    // A new press always owns a voice, including an existing/stealed one.
    voice_channel_[slot]=channel;
    voice_note_id_[slot]=note_id;
    key_held_[slot]=true;
    key_generation_[slot]=next_key_generation_++;
    deferred_release_[slot]=false;
    voices_[slot].set_poly_pressure(0.0f);
    voices_[slot].set_pitch_bend(pitch_bend_);
    voices_[slot].set_fm_amount(fm_amount_);
    voices_[slot].set_osc2_semi(osc2_semi_);
    voices_[slot].set_hard_sync(hard_sync_);
}

void VoiceManager::note_off(int note,int channel,int note_id) {
    if(note<0||note>127)return;
    if(mono_){
        const auto released=mono_keys_.release(note,channel,note_id);
        if(!released.found||!released.was_top)return;
        if(released.has_next){
            const auto& previous=released.next;
            voices_[0].retarget_note(previous.note,previous.velocity,!legato_);
            voice_channel_[0]=previous.channel;
            voice_note_id_[0]=previous.note_id;
            key_held_[0]=true;
            deferred_release_[0]=false;
            voices_[0].set_poly_pressure(0.f);
        }else{
            key_held_[0]=false;
            if(sustain_down_)deferred_release_[0]=true;
            else {voices_[0].note_off();deferred_release_[0]=false;}
        }
        return;
    }
    int chosen=-1;
    uint32_t newest=0;
    // VST3 note IDs are stronger identity than pitch. Without an ID (USB
    // MIDI1 controllers), release ONE physical held press, not every
    // overlapping voice of the same pitch.
    for(size_t i=0;i<MAX_VOICES;++i){
        if(!key_held_[i]||!voices_[i].is_active()||voices_[i].get_note()!=note)
            continue;
        if(channel>=0&&voice_channel_[i]!=channel)continue;
        if(note_id>=0&&voice_note_id_[i]!=note_id)continue;
        if(chosen<0||key_generation_[i]>=newest){
            newest=key_generation_[i];
            chosen=static_cast<int>(i);
        }
    }
    if(chosen<0)return;
    const size_t slot=static_cast<size_t>(chosen);
    key_held_[slot]=false;
    if(sustain_down_)deferred_release_[slot]=true;
    else {voices_[slot].note_off();deferred_release_[slot]=false;}
}

void VoiceManager::set_poly_pressure(int note,int channel,int note_id,float value) {
    if(note<0||note>127||channel<0||channel>15)return;
    const float pressure=clamp(value,0.f,1.f);
    for(size_t i=0;i<MAX_VOICES;++i){
        if(!voices_[i].is_active()||voices_[i].get_note()!=note||
           voice_channel_[i]!=channel)continue;
        if(note_id>=0&&voice_note_id_[i]!=note_id)continue;
        voices_[i].set_poly_pressure(pressure);
    }
}

void VoiceManager::set_sustain_pedal(bool down) {
    if(sustain_down_==down)return;
    sustain_down_=down;
    if(!down){
        // A fresh same-pitch key press has its own unlatched voice, so release
        // the *original* deferred voices even while that pitch is held again.
        for(size_t i=0;i<MAX_VOICES;++i){
            if(deferred_release_[i])voices_[i].note_off();
            deferred_release_[i]=false;
        }
    }
}

void VoiceManager::all_notes_off() {
    sustain_down_=false;
    deferred_release_.fill(false);
    key_held_.fill(false);
    mono_keys_.clear();
    for(auto& voice:voices_)voice.note_off();
}

void VoiceManager::all_sounds_off() {
    sustain_down_=false;
    deferred_release_.fill(false);
    key_held_.fill(false);
    mono_keys_.clear();
    for(auto& voice:voices_)voice.kill();
}

void VoiceManager::set_pitch_bend(float semitones) {
    pitch_bend_ = semitones;
    for (auto& v : voices_) {
        v.set_pitch_bend(semitones);
    }
}

void VoiceManager::set_waveform(Waveform wf) {
    for (auto& v : voices_) {
        v.set_waveform(wf);
    }
}

void VoiceManager::set_sub_mix(float sub_mix) {
    for (auto& v : voices_) {
        v.set_sub_mix(sub_mix);
    }
}
void VoiceManager::set_fundamental_mix(float mix) { for (auto& v : voices_) v.set_fundamental_mix(mix); }
void VoiceManager::set_sub_ratio(int denominator) { for (auto& v : voices_) v.set_sub_ratio(denominator); }
void VoiceManager::set_sub_phase(float phase_cycles) { for (auto& v : voices_) v.set_sub_phase(phase_cycles); }
void VoiceManager::set_sub_polarity(bool inverted) { for (auto& v : voices_) v.set_sub_polarity(inverted); }
void VoiceManager::set_sub_saturation(float amount) { for (auto& v : voices_) v.set_sub_saturation(amount); }
void VoiceManager::set_sub_envelope(float amount) { for (auto& v : voices_) v.set_sub_envelope(amount); }
void VoiceManager::set_voice_mode(bool mono, bool legato) {
    if(mono_!=mono){
        // A mode change changes voice ownership. Clear held keys so a later
        // note-off cannot address the wrong voice after the transition.
        all_sounds_off();
        mono_=mono;
    }
    legato_=legato;
}
void VoiceManager::set_portamento(float seconds) { for (auto& v : voices_) v.set_portamento(seconds); }
void VoiceManager::set_vibrato(float semitones) { for (auto& v : voices_) v.set_vibrato(semitones); }

void VoiceManager::set_noise_mix(float noise_mix) {
    for (auto& v : voices_) {
        v.set_noise_mix(noise_mix);
    }
}

void VoiceManager::set_detune(float detune_cents) {
    for (auto& v : voices_) {
        v.set_detune(detune_cents);
    }
}

void VoiceManager::set_fm_amount(float fm) {
    fm_amount_ = clamp(fm, 0.0f, 1.0f);
    for (auto& v : voices_) {
        v.set_fm_amount(fm_amount_);
    }
}

void VoiceManager::set_osc2_semi(int semi) {
    osc2_semi_ = clamp(semi, -24, 24);
    for (auto& v : voices_) {
        v.set_osc2_semi(osc2_semi_);
    }
}

void VoiceManager::set_hard_sync(bool sync) {
    hard_sync_ = sync;
    for (auto& v : voices_) {
        v.set_hard_sync(hard_sync_);
    }
}

void VoiceManager::set_wave_morph_controls(int family_a, int family_b, float morph,
                                           float phase_warp, float wave_fold,
                                           int unison_voices, float unison_detune_cents,
                                           float sub_harmonics, float env_to_morph) {
    for (auto& v : voices_) {
        v.set_waveform_bank(&bank_);
        v.set_wave_morph_controls(family_a, family_b, morph, phase_warp, wave_fold,
                                  unison_voices, unison_detune_cents, sub_harmonics, env_to_morph);
    }
}

void VoiceManager::set_lfo_morph_mod(float amount) {
    for (auto& v : voices_) {
        v.set_lfo_morph_mod(amount);
    }
}

void VoiceManager::set_generator_params(const WaveformGeneratorParams& params) {
    bank_.regenerate_generator(params);
}

void VoiceManager::set_amp_envelope(float a, float d, float s, float r) {
    for (auto& v : voices_) {
        v.set_env_parameters(a, d, s, r);
    }
}

void VoiceManager::set_filter_envelope(float a, float d, float s, float r) {
    for (auto& v : voices_) {
        v.set_filter_env_parameters(a, d, s, r);
    }
}

float VoiceManager::process(float& out_filter_env) {
    float weight = 0.0f;
    return sanitize(process_split(out_filter_env, weight) + weight);
}

float VoiceManager::process_split(float& out_filter_env, float& out_weight) {
    float sum = 0.0f;
    float weight_sum = 0.0f;
    float env_sum = 0.0f;
    size_t active_count = 0;

    for (auto& v : voices_) {
        if (v.is_active()) {
            float f_env = 0.0f;
            float weight = 0.0f;
            sum += v.process_split(f_env, weight);
            weight_sum += weight;
            env_sum += f_env;
            active_count++;
        }
    }

    if (active_count > 0) {
        out_filter_env = env_sum / static_cast<float>(active_count);
        // Polyphonic scaling to prevent harsh digital clipping
        float headroom_scale = 1.0f / std::sqrt(static_cast<float>(active_count));
        out_weight = sanitize(weight_sum * headroom_scale);
        return sanitize(sum * headroom_scale);
    }

    out_filter_env = 0.0f;
    out_weight = 0.0f;
    return 0.0f;
}

size_t VoiceManager::active_voice_count() const {
    size_t count = 0;
    for (const auto& v : voices_) {
        if (v.is_active()) count++;
    }
    return count;
}

} // namespace monkeys_ear
