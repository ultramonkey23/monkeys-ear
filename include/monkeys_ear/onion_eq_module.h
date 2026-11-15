#pragma once

#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/onion_skin_eq.h"
#include "monkeys_ear/types.h"
#include <array>
#include <cstdint>

namespace monkeys_ear {

enum class OnionRefSource : int {
    Auto = 0,       // Sidechain audio + active Ecosystem Peer
    Sidechain = 1,  // External VST3 sidechain bus (3/4) only
    EcoPeer = 2,    // Shared-memory MEC3 Ecosystem Peer only
    SelfDeHarsh = 3 // Dynamic narrowband self-resonance excess across 24 ERB bands
};

enum class OnionStereoMode : int {
    Stereo = 0,
    MidOnly = 1,
    SideOnly = 2
};

struct OnionEqControls {
    bool bypass = false;
    bool dynamic_enabled = true;
    OnionSkinIntent intent = OnionSkinIntent::Yield;
    float amount = 0.65f;
    float sensitivity = 0.55f;
    float low_protect = 0.70f;
    float transient_protect = 0.60f;
    float tilt_db = 0.0f;
    float q_focus = 1.85f;
    float air_db = 0.0f;
    OnionRefSource ref_source = OnionRefSource::Auto;
    EcoLinkMode eco_mode = EcoLinkMode::Bidirectional;
    int eco_slot = 0;
    float peer_mask_weight = 0.75f;
    OnionStereoMode stereo_mode = OnionStereoMode::Stereo;
    float mix = 1.0f;
    float output_trim_db = 0.0f;
    std::array<float, OnionSkinEQ::kNumBands> manual_db{};
};

struct OnionEqHealth {
    bool finite_output = true;
    bool sidechain_active = false;
    bool eco_peer_active = false;
    bool self_deharsh_active = false;
    float in_peak = 0.0f;
    float out_peak = 0.0f;
};

// Standalone 24-band ERB Onion Skin Dynamic + Surgical EQ FX module.
// Links the canonical OnionSkinEQ dual-channel core with VST3 optional sidechain audio,
// lock-free MEC3 24-ERB Ecosystem Bus, Self De-Harsh mode, M/S processing, Dry/Wet Mix,
// and Output Trim.
class OnionEqModule {
public:
    OnionEqModule() noexcept;

    void prepare(float sample_rate, uint32_t max_block_size) noexcept;
    void reset() noexcept;
    void set_controls(const OnionEqControls& controls) noexcept;

    void process_block(const float* in_l,
                       const float* in_r,
                       const float* sc_l,
                       const float* sc_r,
                       float* out_l,
                       float* out_r,
                       uint32_t num_samples) noexcept;

    const OnionSkinMetrics& metrics() const noexcept { return merged_metrics_; }
    const OnionEqHealth& health() const noexcept { return health_; }

private:
    float sample_rate_ = 48000.0f;
    uint32_t max_block_size_ = 512;
    OnionEqControls controls_{};
    OnionSkinEQ eq_l_{};
    OnionSkinEQ eq_r_{};
    OnionSkinMetrics merged_metrics_{};
    OnionEqHealth health_{};
    EcosystemBusClient eco_bus_{};
    uint64_t instance_tag_ = 0;
    uint64_t pub_seq_ = 0;
    std::array<float, OnionSkinEQ::kNumBands> last_self_occ_{};
};

} // namespace monkeys_ear
