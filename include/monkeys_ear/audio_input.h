#pragma once

#include "monkeys_ear/types.h"
#include "monkeys_ear/vocal_analysis.h"
#include "monkeys_ear/vocal_target.h"

namespace monkeys_ear {
struct VocalExpressionControls { bool enabled=false; float correction_strength=0.0f,drift_retention=1.0f,vibrato_retention=1.0f,transition=0.5f,formant_repair=0.0f,spectral_residual_mix=0.0f,character=0.0f,mix=1.0f,sequential_stage_mix=0.0f,aperiodic_protection=1.0f; VocalTargetControls target{}; };
enum class VocalSourceType : uint8_t { Aperiodic, Mixed, Periodic };
struct VocalExpressionMetrics { float tracked_hz=0.0f,confidence=0.0f,correction_cents=0.0f,voiced_mix=0.0f,residual_mix=0.0f,periodic_mix=0.0f,aperiodic_mix=1.0f,selected_target_cents=0.0f,target_trajectory_cents=0.0f; int selected_degree=-1; VocalSourceType source_type=VocalSourceType::Aperiodic; bool voiced=false; };

class AudioInputProcessor {
public:
    static constexpr size_t RING_BUFFER_SIZE=8192;
    AudioInputProcessor();
    void set_sample_rate(float sr); void set_gain(float gain_db); void set_mix(float mix); void set_highpass_enabled(bool en); void set_vocal_controls(const VocalExpressionControls& controls); void reset();
    float process_sample(float mic_in,float synth_in); float process_vocal_sample(float input,float tracked_hz,float confidence);
    float get_rms_level() const{return rms_level_;} float get_peak_level() const{return peak_level_;} float read_ring_buffer(size_t lag) const{return ring_buffer_.read(lag);}
    const VocalExpressionMetrics& vocal_metrics() const{return vocal_metrics_;} const VocalAnalysisFrame& vocal_analysis() const{return vocal_analysis_;}
private:
    struct RuntimeVocalControls { float correction_strength=0.0f,drift_retention=1.0f,vibrato_retention=1.0f,transition=0.5f,formant_repair=0.0f,spectral_residual_mix=0.0f,character=0.0f,mix=1.0f,sequential_stage_mix=0.0f,aperiodic_protection=1.0f; } runtime_vocal_{};
    static VocalExpressionControls sanitize_vocal_controls(const VocalExpressionControls&) noexcept; void snap_runtime_vocal_controls() noexcept; void advance_runtime_vocal_controls() noexcept;
    float sample_rate_,gain_linear_,mix_; bool highpass_enabled_; bool runtime_vocal_initialized_=false; bool pitch_state_valid_=false;
    LockFreeRingBuffer<float,RING_BUFFER_SIZE> ring_buffer_,vocal_stage_ring_buffer_;
    float hp_x1_,hp_y1_,rms_accumulator_,rms_level_,peak_level_; size_t rms_count_;
    VocalExpressionControls vocal_controls_{}; VocalExpressionMetrics vocal_metrics_{}; VocalAnalysisFrame vocal_analysis_{};
    float grain_phase_=0.0f,secondary_grain_phase_=0.0f,correction_cents_=0.0f,pitch_fast_cents_=0.0f,pitch_slow_cents_=0.0f;
    float source_envelope_=0.0f,shifted_envelope_=0.0f,residual_low_=0.0f,analysis_low_=0.0f,periodic_energy_=0.0f,aperiodic_energy_=0.0f,onset_fast_=0.0f,onset_slow_=0.0f;
    VocalTargetEngine vocal_target_{};
};
} // namespace monkeys_ear
