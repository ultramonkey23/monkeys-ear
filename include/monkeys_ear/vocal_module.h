#pragma once

#include "monkeys_ear/module_contract.h"
#include "monkeys_ear/ecosystem_state.h"
#include "monkeys_ear/audio_input.h"
#include "monkeys_ear/tonal.h"
#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/onion_skin_eq.h"
#include <cstdint>

namespace monkeys_ear {

// Vocal pitch/intonation/timbre plugin module layered over AudioInputProcessor
// without altering the synth/FX core or creating a separate DSP implementation.
class VocalModule final : public StandaloneModule {
public:
    static constexpr const char* kModuleId = "monkeys-ear.vocal";

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
    void set_eco_slot(int slot) noexcept;
    int eco_slot() const noexcept { return eco_slot_; }
    void set_eco_mode(int mode) noexcept;
    int eco_mode() const noexcept { return eco_mode_; }
    void set_eco_link_enabled(bool enabled) noexcept { set_eco_mode(enabled ? 3 : 0); }
    bool eco_link_enabled() const noexcept { return eco_mode_ != 0; }
    bool is_eco_peer_linked() const noexcept { return eco_peer_linked_; }
    float active_tracked_hz() const noexcept { return last_tracked_hz_; }
    float active_confidence() const noexcept { return last_tracked_conf_; }

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
    uint32_t shared_pitch_samples_remaining_ = 0;
    ExternalSubharmonic local_pitch_tracker_{};
    bool shared_pitch_valid_ = false;
    bool bypass_ = false;

    EcosystemBusClient eco_bus_{};
    OnionSkinEQ eco_analyzer_{};
    int eco_slot_ = 0;
    int eco_mode_ = 3; // 0=Off, 1=Publish, 2=Follow, 3=Bidirectional
    bool eco_link_enabled_ = true;
    bool eco_peer_linked_ = false;
    uint64_t last_eco_synth_frame_id_ = 0;
    uint64_t vocal_pub_frame_id_ = 0;
    float last_tracked_hz_ = 0.0f;
    float last_tracked_conf_ = 0.0f;
};

} // namespace monkeys_ear
