#include "monkeys_ear/audio_input.h"
#include <cmath>

namespace monkeys_ear {
namespace {
float unit(float x) noexcept { return clamp(sanitize(x), 0.0f, 1.0f); }
float smooth_toward(float current, float target, float alpha) noexcept {
    return current + (target - current) * alpha;
}
}

AudioInputProcessor::AudioInputProcessor()
    : sample_rate_(48000.0f), gain_linear_(1.0f), mix_(0.0f), highpass_enabled_(true),
      hp_x1_(0.0f), hp_y1_(0.0f), rms_accumulator_(0.0f), rms_level_(0.0f), peak_level_(0.0f), rms_count_(0) {}

VocalExpressionControls AudioInputProcessor::sanitize_vocal_controls(const VocalExpressionControls& in) noexcept {
    VocalExpressionControls c = in;
    c.correction_strength = unit(c.correction_strength); c.drift_retention = unit(c.drift_retention);
    c.vibrato_retention = unit(c.vibrato_retention); c.transition = unit(c.transition);
    c.formant_repair = unit(c.formant_repair); c.spectral_residual_mix = unit(c.spectral_residual_mix);
    c.character = unit(c.character); c.mix = unit(c.mix); c.sequential_stage_mix = unit(c.sequential_stage_mix);
    c.aperiodic_protection = unit(c.aperiodic_protection);
    c.target.articulation = clamp(sanitize(c.target.articulation), -1.0f, 1.0f);
    c.target.portamento = unit(c.target.portamento); c.target.transition_preservation = unit(c.target.transition_preservation);
    c.target.target_hysteresis = unit(c.target.target_hysteresis); c.target.onset_protection = unit(c.target.onset_protection);
    c.target.directionality = unit(c.target.directionality);
    return c;
}

void AudioInputProcessor::snap_runtime_vocal_controls() noexcept {
    runtime_vocal_.correction_strength = vocal_controls_.correction_strength;
    runtime_vocal_.drift_retention = vocal_controls_.drift_retention;
    runtime_vocal_.vibrato_retention = vocal_controls_.vibrato_retention;
    runtime_vocal_.transition = vocal_controls_.transition;
    runtime_vocal_.formant_repair = vocal_controls_.formant_repair;
    runtime_vocal_.spectral_residual_mix = vocal_controls_.spectral_residual_mix;
    runtime_vocal_.character = vocal_controls_.character; runtime_vocal_.mix = vocal_controls_.mix;
    runtime_vocal_.sequential_stage_mix = vocal_controls_.sequential_stage_mix;
    runtime_vocal_.aperiodic_protection = vocal_controls_.aperiodic_protection;
    runtime_vocal_initialized_ = true;
}

void AudioInputProcessor::advance_runtime_vocal_controls() noexcept {
    if (!runtime_vocal_initialized_) { snap_runtime_vocal_controls(); return; }
    // Five milliseconds is short enough for performance automation but avoids
    // sample-discontinuous gain/ratio changes in the nonlinear/resynthesis path.
    const float alpha = 1.0f - std::exp(-1.0f / (0.005f * sample_rate_));
    runtime_vocal_.correction_strength = smooth_toward(runtime_vocal_.correction_strength, vocal_controls_.correction_strength, alpha);
    runtime_vocal_.drift_retention = smooth_toward(runtime_vocal_.drift_retention, vocal_controls_.drift_retention, alpha);
    runtime_vocal_.vibrato_retention = smooth_toward(runtime_vocal_.vibrato_retention, vocal_controls_.vibrato_retention, alpha);
    runtime_vocal_.transition = smooth_toward(runtime_vocal_.transition, vocal_controls_.transition, alpha);
    runtime_vocal_.formant_repair = smooth_toward(runtime_vocal_.formant_repair, vocal_controls_.formant_repair, alpha);
    runtime_vocal_.spectral_residual_mix = smooth_toward(runtime_vocal_.spectral_residual_mix, vocal_controls_.spectral_residual_mix, alpha);
    runtime_vocal_.character = smooth_toward(runtime_vocal_.character, vocal_controls_.character, alpha);
    runtime_vocal_.mix = smooth_toward(runtime_vocal_.mix, vocal_controls_.mix, alpha);
    runtime_vocal_.sequential_stage_mix = smooth_toward(runtime_vocal_.sequential_stage_mix, vocal_controls_.sequential_stage_mix, alpha);
    runtime_vocal_.aperiodic_protection = smooth_toward(runtime_vocal_.aperiodic_protection, vocal_controls_.aperiodic_protection, alpha);
}

void AudioInputProcessor::set_sample_rate(float sr) {
    sr = sanitize(sr); sample_rate_ = sr >= 1000.0f ? clamp(sr, 1000.0f, 768000.0f) : 48000.0f;
    vocal_target_.set_sample_rate(sample_rate_);
}
void AudioInputProcessor::set_vocal_controls(const VocalExpressionControls& controls) {
    const bool was_enabled = vocal_controls_.enabled;
    vocal_controls_ = sanitize_vocal_controls(controls);
    vocal_target_.set_controls(vocal_controls_.target);
    if (!runtime_vocal_initialized_ || (!was_enabled && vocal_controls_.enabled)) snap_runtime_vocal_controls();
}
void AudioInputProcessor::set_gain(float gain_db) { gain_linear_ = std::pow(10.0f, clamp(sanitize(gain_db), -24.0f, 24.0f) / 20.0f); }
void AudioInputProcessor::set_mix(float mix) { mix_ = unit(mix); }
void AudioInputProcessor::set_highpass_enabled(bool en) { highpass_enabled_ = en; }

void AudioInputProcessor::reset() {
    ring_buffer_.reset(); vocal_stage_ring_buffer_.reset(); hp_x1_ = hp_y1_ = rms_accumulator_ = rms_level_ = peak_level_ = 0.0f; rms_count_ = 0;
    grain_phase_ = secondary_grain_phase_ = correction_cents_ = pitch_fast_cents_ = pitch_slow_cents_ = 0.0f;
    source_envelope_ = shifted_envelope_ = residual_low_ = analysis_low_ = periodic_energy_ = aperiodic_energy_ = onset_fast_ = onset_slow_ = 0.0f;
    vocal_target_.reset(); vocal_metrics_ = {}; vocal_analysis_ = {}; runtime_vocal_initialized_ = false; snap_runtime_vocal_controls();
}

float AudioInputProcessor::process_vocal_sample(float input, float tracked_hz, float confidence) {
    input = sanitize(input); tracked_hz = sanitize(tracked_hz); confidence = sanitize(confidence);
    if (!vocal_controls_.enabled) { vocal_metrics_ = {}; vocal_analysis_ = {}; return input; }
    advance_runtime_vocal_controls(); const auto& c = runtime_vocal_;
    const float hz = clamp(tracked_hz, 55.0f, 900.0f), conf = unit(confidence);
    const bool voiced = tracked_hz >= 55.0f && tracked_hz <= 900.0f && conf > 0.35f;
    const float confidence_mix = voiced ? clamp((conf - 0.35f) / 0.55f, 0.0f, 1.0f) : 0.0f;

    const float analysis_alpha = 1.0f - std::exp(-TWO_PI * 1200.0f / sample_rate_);
    analysis_low_ += (input - analysis_low_) * analysis_alpha;
    const float energy_alpha = 1.0f - std::exp(-1.0f / (0.012f * sample_rate_));
    periodic_energy_ += (std::abs(analysis_low_) - periodic_energy_) * energy_alpha;
    aperiodic_energy_ += (std::abs(input - analysis_low_) - aperiodic_energy_) * energy_alpha;
    const float tonal_balance = periodic_energy_ / std::max(0.0001f, periodic_energy_ + aperiodic_energy_);
    const float periodic_mix = confidence_mix * clamp((tonal_balance - 0.12f) / 0.74f, 0.0f, 1.0f), aperiodic_mix = 1.0f - periodic_mix;
    const VocalSourceType source_type = periodic_mix > 0.65f ? VocalSourceType::Periodic : (periodic_mix < 0.18f ? VocalSourceType::Aperiodic : VocalSourceType::Mixed);
    vocal_analysis_ = {}; vocal_analysis_.pitch_confidence = conf; vocal_analysis_.periodic_weight = periodic_mix;
    vocal_analysis_.mixed_weight = source_type == VocalSourceType::Mixed ? std::min(periodic_mix, aperiodic_mix) : 0.0f;
    vocal_analysis_.aperiodic_weight = aperiodic_mix; vocal_analysis_.peak = std::abs(input); vocal_analysis_.rms = rms_level_;
    if (!voiced) { vocal_analysis_.normalize_source_weights(); vocal_analysis_.sanitize(); vocal_metrics_ = {tracked_hz,conf,correction_cents_,0,0,0,1,0,0,-1,VocalSourceType::Aperiodic,false}; return input; }

    const float input_cents = 1200.0f * std::log2(hz / 440.0f);
    const float fast_alpha = 1.0f - std::exp(-1.0f / (0.008f * sample_rate_)), slow_alpha = 1.0f - std::exp(-1.0f / (0.110f * sample_rate_));
    pitch_fast_cents_ += (input_cents - pitch_fast_cents_) * fast_alpha; pitch_slow_cents_ += (pitch_fast_cents_ - pitch_slow_cents_) * slow_alpha;
    const float pitch_motion_residual = pitch_fast_cents_ - pitch_slow_cents_;
    const float onset_fast_alpha = 1.0f - std::exp(-1.0f / (0.003f * sample_rate_)), onset_slow_alpha = 1.0f - std::exp(-1.0f / (0.035f * sample_rate_));
    onset_fast_ += (std::abs(input) - onset_fast_) * onset_fast_alpha; onset_slow_ += (std::abs(input) - onset_slow_) * onset_slow_alpha;
    const float onset = clamp((onset_fast_ - onset_slow_) / std::max(0.02f, onset_slow_ * 1.5f), 0.0f, 1.0f), motion = pitch_motion_residual / 0.110f;
    const float target = vocal_target_.process(pitch_slow_cents_, motion, conf, onset);
    vocal_analysis_.observed_cents=input_cents; vocal_analysis_.center_cents=pitch_slow_cents_; vocal_analysis_.residual_cents=pitch_motion_residual;
    vocal_analysis_.motion_cents_per_second=motion; vocal_analysis_.onset=onset; vocal_analysis_.stability=1.0f-clamp(std::abs(motion)/600.0f,0.0f,1.0f);
    vocal_analysis_.motion_intent=clamp(std::abs(motion)/300.0f,0.0f,1.0f); vocal_analysis_.normalize_source_weights(); vocal_analysis_.sanitize();

    const float retained_pitch = pitch_slow_cents_ + pitch_motion_residual * c.vibrato_retention;
    const float desired_cents = (target - retained_pitch) * c.correction_strength * (1.0f - c.drift_retention);
    const float transition_ms = 2.0f + (1.0f - c.transition) * 58.0f;
    correction_cents_ += (desired_cents - correction_cents_) * (1.0f - std::exp(-1.0f / (transition_ms * 0.001f * sample_rate_)));
    // Bound transform authority before exp2/pow. This prevents hostile tuning or
    // detector discontinuities from creating pathological grain phase steps.
    correction_cents_ = clamp(sanitize(correction_cents_), -2400.0f, 2400.0f);

    const float period = clamp(sample_rate_ / hz, 53.0f, 872.0f), sequential_mix = c.sequential_stage_mix;
    const float primary_cents = correction_cents_ * (1.0f - 0.5f * sequential_mix), secondary_cents = correction_cents_ - primary_cents;
    const float primary_ratio = clamp(std::pow(2.0f, primary_cents / 1200.0f), 0.25f, 4.0f);
    grain_phase_ -= primary_ratio / period; grain_phase_ -= std::floor(grain_phase_);
    const float phase_b = grain_phase_ < 0.5f ? grain_phase_ + 0.5f : grain_phase_ - 0.5f;
    const float base_lag = clamp(period * 1.15f, 64.0f, static_cast<float>(RING_BUFFER_SIZE - 900));
    const float wa=std::sin(PI*grain_phase_), wb=std::sin(PI*phase_b);
    const float psola_primary=ring_buffer_.read_interpolated(base_lag+grain_phase_*period)*wa*wa+ring_buffer_.read_interpolated(base_lag+phase_b*period)*wb*wb;
    vocal_stage_ring_buffer_.push(psola_primary);
    const float secondary_ratio = clamp(std::pow(2.0f, secondary_cents / 1200.0f), 0.25f, 4.0f);
    secondary_grain_phase_ -= secondary_ratio / period; secondary_grain_phase_ -= std::floor(secondary_grain_phase_);
    const float phase_second_b=secondary_grain_phase_<0.5f?secondary_grain_phase_+0.5f:secondary_grain_phase_-0.5f;
    const float ws0=std::sin(PI*secondary_grain_phase_),ws1=std::sin(PI*phase_second_b);
    const float psola_secondary=vocal_stage_ring_buffer_.read_interpolated(base_lag+secondary_grain_phase_*period)*ws0*ws0+vocal_stage_ring_buffer_.read_interpolated(base_lag+phase_second_b*period)*ws1*ws1;
    const float psola=lerp(psola_primary,psola_secondary,sequential_mix);

    const float env_alpha=1.0f-std::exp(-1.0f/(0.018f*sample_rate_)); source_envelope_+=(std::abs(input)-source_envelope_)*env_alpha; shifted_envelope_+=(std::abs(psola)-shifted_envelope_)*env_alpha;
    const float repair=lerp(1.0f,clamp(source_envelope_/std::max(0.015f,shifted_envelope_),0.65f,1.55f),c.formant_repair),repaired=psola*repair;
    residual_low_+=(input-residual_low_)*(1.0f-std::exp(-TWO_PI*1700.0f/sample_rate_));
    const float residual_request=std::min(0.25f,c.spectral_residual_mix*0.25f)*(1.0f-periodic_mix*0.35f),protected_aperiodic=aperiodic_mix*c.aperiodic_protection;
    const float transform_mix=periodic_mix*(1.0f-protected_aperiodic*0.82f),residual_mix=std::min(residual_request,1.0f-transform_mix);
    const float dual_layer=repaired*transform_mix+(input-residual_low_)*residual_mix+input*(1.0f-transform_mix-residual_mix);
    const float colored=lerp(dual_layer,fast_tanh(dual_layer*(1.0f+c.character*2.5f)),c.character*0.35f);
    const auto& tm=vocal_target_.metrics(); vocal_metrics_={tracked_hz,conf,correction_cents_,confidence_mix,residual_mix,periodic_mix,aperiodic_mix,tm.selected_cents,tm.trajectory_cents,tm.selected_degree,source_type,voiced};
    return sanitize(lerp(input,colored,c.mix));
}

float AudioInputProcessor::process_sample(float mic_in, float synth_in) {
    float conditioned=sanitize(mic_in)*gain_linear_; synth_in=sanitize(synth_in);
    if(highpass_enabled_){const float r=clamp(1.0f-(TWO_PI*20.0f/sample_rate_),0.0f,0.99999f);const float hp_out=conditioned-hp_x1_+r*hp_y1_;hp_x1_=conditioned;hp_y1_=sanitize(hp_out);conditioned=hp_y1_;}
    conditioned=fast_tanh(conditioned); ring_buffer_.push(conditioned); const float abs_val=std::abs(conditioned);
    if(abs_val>peak_level_)peak_level_=abs_val;else peak_level_*=0.9995f; rms_accumulator_+=conditioned*conditioned;rms_count_++;
    if(rms_count_>=512){rms_level_=sanitize(std::sqrt(std::max(0.0f,rms_accumulator_)/512.0f));rms_accumulator_=0.0f;rms_count_=0;}
    return sanitize(lerp(synth_in,conditioned,mix_));
}

} // namespace monkeys_ear
