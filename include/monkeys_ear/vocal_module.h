#pragma once

#include "monkeys_ear/module_contract.h"
#include "monkeys_ear/ecosystem_state.h"
#include "monkeys_ear/audio_input.h"
#include "monkeys_ear/tonal.h"
#include <cstdint>

namespace monkeys_ear {

// Vocal remains one Monkey's Ear ecosystem module. AudioInputProcessor is the
// canonical vocal DSP; this boundary adds module lifecycle and evidence policy.
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

    // Local evidence keeps the plugin useful alone. A connected ecosystem may
    // supply fresher/more-confident compatible evidence without replacing the
    // local capability or creating a second DSP implementation.
    void set_local_pitch_evidence(float tracked_hz, float confidence) noexcept;
    void consume_ecosystem(const EcosystemSnapshot<32>& snapshot) noexcept;
    void clear_ecosystem_evidence() noexcept;

    void process_block(const float* input_l, const float* input_r,
                       float* output_l, float* output_r, uint32_t num_samples) noexcept;

    const VocalExpressionMetrics& metrics() const noexcept { return processor_.vocal_metrics(); }
    const VocalAnalysisFrame& analysis() const noexcept { return processor_.vocal_analysis(); }

private:
    static const ModuleIdentity kIdentity;
    AudioInputProcessor processor_{};
    VocalExpressionControls controls_{};
    ModuleHealth health_{};
    float sample_rate_ = 48000.0f;
    uint32_t max_block_size_ = 512;
    float local_pitch_hz_ = 0.0f;
    float local_pitch_confidence_ = 0.0f;
    float shared_pitch_hz_ = 0.0f;
    float shared_pitch_confidence_ = 0.0f;
    ExternalSubharmonic local_pitch_tracker_{};
    bool shared_pitch_valid_ = false;
    bool bypass_ = false;
};

} // namespace monkeys_ear
