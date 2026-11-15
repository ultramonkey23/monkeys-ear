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

void PolyBLEPOscillator::set_morph_curve(int curve) {
    morph_curve_ = clamp(curve, 0, 3);
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
    // 2x Oversampled Wavefolder/Warp anti-aliasing when fold/warp nonlinearity is active
    if (wave_fold_ > 0.02f || std::abs(phase_warp_ - 0.5f) > 0.05f) {
        const float half_dt = dt * 0.5f;
        const float s0 = active_bank.sample_morph(family_a_, family_b_, eff_morph, eff_phase, half_dt, phase_warp_, wave_fold_, morph_curve_);
        const float s1 = active_bank.sample_morph(family_a_, family_b_, eff_morph, eff_phase + half_dt, half_dt, phase_warp_, wave_fold_, morph_curve_);
        return 0.5f * (s0 + s1);
    }
    return active_bank.sample_morph(family_a_, family_b_, eff_morph, eff_phase, dt, phase_warp_, wave_fold_, morph_curve_);
}

float PolyBLEPOscillator::process() {
    return process_with_pm(0.0f, 0.0f);
}

float PolyBLEPOscillator::process_with_pm(float phase_mod) {
    return process_with_pm(phase_mod, 0.0f);
}

float PolyBLEPOscillator::process_with_pm(float phase_mod, float morph_mod) {
    float l = 0.0f, r = 0.0f;
    process_with_pm_stereo(phase_mod, morph_mod, 0.0f, l, r);
    return sanitize(0.5f * (l + r));
}

void PolyBLEPOscillator::process_with_pm_stereo(float phase_mod, float morph_mod, float width01, float& out_l, float& out_r) {
    const float dt = phase_increment_;
    const float eff_morph = clamp(morph_ + morph_mod, 0.0f, 1.0f);
    const float w = clamp(width01, 0.0f, 1.0f);

    if (unison_voices_ <= 1) {
        float out = eval_single_phase(phase_ + phase_mod, dt, eff_morph);
        if (std::abs(sync_blep_pending_) > 1e-5f) {
            out += sync_blep_pending_ * poly_blep(phase_, std::max(1e-5f, dt));
            sync_blep_pending_ = 0.0f;
        }
        last_sample_ = out;
        out_l = sanitize(out);
        out_r = out_l;
    } else {
        const float half = 0.5f * static_cast<float>(unison_voices_ - 1);
        float sum_l = 0.0f, sum_r = 0.0f;
        const float center_s = eval_single_phase(phase_ + phase_mod, dt, eff_morph);
        sum_l += center_s;
        sum_r += center_s;
        for (int u = 1; u < unison_voices_; ++u) {
            const float offset_index = static_cast<float>(u) - half;
            const float pan = (half > 0.01f) ? clamp((offset_index / half) * w, -1.0f, 1.0f) : 0.0f;
            const float detune_ratio = std::pow(2.0f, (offset_index * unison_detune_cents_) / (1200.0f * std::max(1.0f, half)));
            const float u_dt = dt * detune_ratio;
            const float u_s = eval_single_phase(unison_phases_[static_cast<size_t>(u)] + phase_mod, u_dt, eff_morph);
            const float gl = std::sqrt(clamp(0.5f * (1.0f - pan), 0.0f, 1.0f));
            const float gr = std::sqrt(clamp(0.5f * (1.0f + pan), 0.0f, 1.0f));
            sum_l += u_s * gl * 1.41421356f;
            sum_r += u_s * gr * 1.41421356f;
            unison_phases_[static_cast<size_t>(u)] += u_dt;
            if (unison_phases_[static_cast<size_t>(u)] >= 1.0f) {
                unison_phases_[static_cast<size_t>(u)] -= std::floor(unison_phases_[static_cast<size_t>(u)]);
            }
        }
        const float norm = 1.0f / std::sqrt(static_cast<float>(unison_voices_));
        out_l = sanitize(sum_l * norm);
        out_r = sanitize(sum_r * norm);
        last_sample_ = 0.5f * (out_l + out_r);
    }

    phase_ += phase_increment_;
    if (phase_ >= 1.0f) {
        wrap_fraction_ = clamp((phase_ - 1.0f) / std::max(1e-6f, phase_increment_), 0.0f, 1.0f);
        phase_ -= std::floor(phase_);
        wrapped_ = true;
    } else {
        wrap_fraction_ = 0.0f;
        wrapped_ = false;
    }
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
    if (osc2_wave_ > 0) {
        const WaveFamily f2 = static_cast<WaveFamily>(clamp(osc2_wave_ - 1, 0, kWaveFamilyCount - 1));
        osc2_.set_wave_families(f2, fb);
    } else {
        osc2_.set_wave_families(fa, fb);
    }
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

void SynthVoice::set_deep_osc_controls(int osc2_wave, float osc2_fine_cents, float ring_mod_mix,
                                       float unison_width, float analog_drift,
                                       float morph_rate_hz, int morph_curve, float osc_balance) {
    osc2_wave_ = clamp(osc2_wave, 0, kWaveFamilyCount);
    osc2_fine_cents_ = clamp(osc2_fine_cents, -50.0f, 50.0f);
    ring_mod_mix_ = clamp(ring_mod_mix, 0.0f, 1.0f);
    unison_width_ = clamp(unison_width, 0.0f, 1.0f);
    analog_drift_ = clamp(analog_drift, 0.0f, 1.0f);
    morph_rate_hz_ = clamp(morph_rate_hz, 0.0f, 8.0f);
    morph_curve_ = clamp(morph_curve, 0, 3);
    osc_balance_ = clamp(osc_balance, 0.0f, 1.0f);

    osc1_.set_morph_curve(morph_curve_);
    osc2_.set_morph_curve(morph_curve_);
    if (osc2_wave_ > 0) {
        const WaveFamily f2 = static_cast<WaveFamily>(clamp(osc2_wave_ - 1, 0, kWaveFamilyCount - 1));
        osc2_.set_wave_families(f2, f2);
    }
    update_frequencies();
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
    float fine_total = detune_cents_ + (osc2_fine_cents_ - 7.0f);
    float f2_ratio = std::pow(2.0f, (static_cast<float>(osc2_semi_) + fine_total / 100.0f) / 12.0f);
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
    float l = 0.0f, r = 0.0f;
    process_stereo_split(filter_env_out, weight_out, l, r);
    return sanitize(0.5f * (l + r));
}

void SynthVoice::process_stereo_split(float& filter_env_out, float& weight_out, float& out_l, float& out_r) {
    if (!is_active()) {
        filter_env_out = 0.0f;
        weight_out = 0.0f;
        out_l = 0.0f;
        out_r = 0.0f;
        return;
    }

    const float inv_sr = 1.0f / std::max(1.0f, sample_rate_);
    age_ += inv_sr;
    if (portamento_seconds_ > 0.0001f) {
        float glide = 1.0f - std::exp(-1.0f / (portamento_seconds_ * sample_rate_));
        current_frequency_ += (target_frequency_ - current_frequency_) * glide;
    } else current_frequency_ = target_frequency_;

    // Subtle analog VCO thermal pitch wander + voice-seeded phase offset
    float drift_cents_1 = 0.0f;
    float drift_cents_2 = 0.0f;
    if (analog_drift_ > 0.001f) {
        const float seed_off = static_cast<float>((note_ * 37 + 11) % 97) * 0.064f;
        drift_phase_ += TWO_PI * 0.42f * inv_sr;
        if (drift_phase_ >= TWO_PI) drift_phase_ -= TWO_PI;
        drift_cents_1 = std::sin(drift_phase_ + seed_off) * (analog_drift_ * 14.0f);
        drift_cents_2 = std::cos(drift_phase_ * 1.37f + seed_off * 1.9f) * (analog_drift_ * 18.0f);
    }

    // Per-voice auto-morph oscillator (0..8 Hz triangle sweep modulation)
    float auto_morph = 0.0f;
    if (morph_rate_hz_ > 0.005f) {
        auto_morph_phase_ += morph_rate_hz_ * inv_sr;
        if (auto_morph_phase_ >= 1.0f) auto_morph_phase_ -= std::floor(auto_morph_phase_);
        const float tri = 1.0f - std::abs(2.0f * auto_morph_phase_ - 1.0f); // [0..1]
        auto_morph = (tri - 0.5f) * 0.85f;
    }

    float f1 = current_frequency_ * std::pow(2.0f, (vibrato_semitones_ + drift_cents_1 / 100.0f) / 12.0f);
    float fine_total = detune_cents_ + (osc2_fine_cents_ - 7.0f) + drift_cents_2;
    osc1_.set_frequency(f1);
    osc2_.set_frequency(f1 * std::pow(2.0f, (static_cast<float>(osc2_semi_) + fine_total / 100.0f) / 12.0f));
    sub_osc_.set_frequency(f1 / static_cast<float>(sub_ratio_denominator_));
    fundamental_osc_.set_frequency(f1);

    // Modulator Oscillator (Osc 2) also tracks dynamic morph modulation
    float env_morph = env_to_morph_ * filter_env_.get_value();
    float dyn_morph = env_morph + lfo_morph_mod_ + auto_morph;
    float s2 = osc2_.process_with_pm(0.0f, dyn_morph);

    // Hard sync: sub-sample phase reset with PolyBLEP discontinuity compensation
    if (hard_sync_ && osc2_.has_wrapped()) {
        osc1_.sync_reset_subsample(osc2_.wrap_fraction());
    }

    // Carrier Oscillator (Osc 1) with Cross-FM Phase Modulation + True Stereo Unison Panning
    float pm_input = (fm_amount_ > 0.001f) ? (s2 * fm_amount_ * 3.5f) : 0.0f;
    float s1_l = 0.0f, s1_r = 0.0f;
    osc1_.process_with_pm_stereo(pm_input, dyn_morph, unison_width_, s1_l, s1_r);

    float s_sub = sub_osc_.process();
    float s_fundamental = fundamental_osc_.process();
    float noise = next_noise();

    // Balance 1<->2 + Ring Modulation / AM harmonic sidebands
    const float bal = clamp(osc_balance_, 0.0f, 1.0f);
    const float osc1_gain = 1.10f * (1.0f - bal * 0.82f);
    const float osc2_gain = 0.90f * (0.18f + bal * 0.82f) * (1.0f - fm_amount_ * 0.35f)
        * (1.0f + 0.45f * poly_pressure_); // per-key expressive harmonic emphasis

    float char_l = osc1_gain * s1_l + osc2_gain * s2;
    float char_r = osc1_gain * s1_r + osc2_gain * s2;
    if (ring_mod_mix_ > 0.001f) {
        const float rm_l = s1_l * s2;
        const float rm_r = s1_r * s2;
        char_l = lerp(char_l, rm_l, ring_mod_mix_);
        char_r = lerp(char_r, rm_r, ring_mod_mix_);
    }
    char_l += noise_mix_ * noise;
    char_r += noise_mix_ * noise;

    float amp = amp_env_.process();
    filter_env_out = filter_env_.process();
    float sub_env = lerp(1.0f, amp, sub_envelope_amount_);
    float sub = s_sub * sub_polarity_;
    if (sub_saturation_ > 0.0f) sub = lerp(sub, fast_tanh(sub * (1.0f + 4.0f*sub_saturation_)), sub_saturation_);
    weight_out = sanitize((fundamental_mix_ * s_fundamental + sub_mix_ * sub) * sub_env);
    out_l = sanitize(char_l * amp);
    out_r = sanitize(char_r * amp);
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

void VoiceManager::set_deep_osc_controls(int osc2_wave, float osc2_fine_cents, float ring_mod_mix,
                                         float unison_width, float analog_drift,
                                         float morph_rate_hz, int morph_curve, float osc_balance) {
    for (auto& v : voices_) {
        v.set_deep_osc_controls(osc2_wave, osc2_fine_cents, ring_mod_mix,
                                unison_width, analog_drift, morph_rate_hz, morph_curve, osc_balance);
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
    float l = 0.0f, r = 0.0f;
    process_stereo_split(out_filter_env, out_weight, l, r);
    return sanitize(0.5f * (l + r));
}

void VoiceManager::process_stereo_split(float& out_filter_env, float& out_weight, float& out_l, float& out_r) {
    float sum_l = 0.0f;
    float sum_r = 0.0f;
    float weight_sum = 0.0f;
    float env_sum = 0.0f;
    size_t active_count = 0;

    for (auto& v : voices_) {
        if (v.is_active()) {
            float f_env = 0.0f;
            float weight = 0.0f;
            float vl = 0.0f, vr = 0.0f;
            v.process_stereo_split(f_env, weight, vl, vr);
            sum_l += vl;
            sum_r += vr;
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
        out_l = sanitize(sum_l * headroom_scale);
        out_r = sanitize(sum_r * headroom_scale);
        return;
    }

    out_filter_env = 0.0f;
    out_weight = 0.0f;
    out_l = 0.0f;
    out_r = 0.0f;
}

size_t VoiceManager::active_voice_count() const {
    size_t count = 0;
    for (const auto& v : voices_) {
        if (v.is_active()) count++;
    }
    return count;
}

} // namespace monkeys_ear
