#pragma once

#include "monkeys_ear/module_contract.h"
#include "monkeys_ear/audio_input.h"
#include <cstdint>

namespace monkeys_ear {

// Thin standalone owner around the existing vocal DSP. It deliberately does
// not duplicate pitch/resynthesis code: suite and standalone wrappers share
// AudioInputProcessor as the single DSP implementation.
class VocalModule final : public StandaloneModule {
public:
    VocalModule() noexcept;

    const ModuleIdentity& identity() const noexcept override;
    ModuleIOContract io_contract() const noexcept override;
    void prepare(float sample_rate, uint32_t max_block_size) noexcept override;
    void reset() noexcept override;
    void set_bypass(bool bypass) noexcept override { bypass_ = bypass; }
    bool bypassed() const noexcept override { return bypass_; }
    ModuleHealth health() const noexcept override { return health_; }

    void set_controls(const VocalExpressionControls& controls) noexcept;
    const VocalExpressionControls& controls() const noexcept { return controls_; }

    // Host/pitch detector supplies causal F0 evidence. Stereo uses a shared
    // analysis trajectory so left/right channels cannot select different notes.
    void process_block(const float* input_l, const float* input_r,
                       float* output_l, float* output_r, uint32_t num_samples,
                       float tracked_hz, float confidence) noexcept;

    const VocalExpressionMetrics& metrics() const noexcept { return processor_.vocal_metrics(); }
    const VocalAnalysisFrame& analysis() const noexcept { return processor_.vocal_analysis(); }

private:
    static const ModuleIdentity kIdentity;
    AudioInputProcessor processor_{};
    VocalExpressionControls controls_{};
    ModuleHealth health_{};
    float sample_rate_ = 48000.0f;
    uint32_t max_block_size_ = 512;
    bool bypass_ = false;
};

} // namespace monkeys_ear
