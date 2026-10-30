#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <cstddef>

namespace monkeys_ear {

// All existing host IDs 0..123 remain stable. 124..160 are reserved for the
// integrated four-tube rack; shared by instrument and FX wrappers.
constexpr int kTubeRackParamBase = 124;
constexpr int kTubeRackGlobalParams = 5;
constexpr int kTubeRackSlotParams = 8;
constexpr int kTubeRackSlotCount = 4;
constexpr int kTubeRackParamCount = kTubeRackGlobalParams + kTubeRackSlotParams * kTubeRackSlotCount;

// Tube labels describe creative, phenomenological transfer-function families.
// They are not measured or calibrated equivalents to specific manufacturers.
constexpr int kTubeFlavorCount = 14;
inline constexpr std::array<const char*,kTubeFlavorCount> kTubeFlavorNames{{
    "12AX7 / ECC83","12AT7 / ECC81","12AY7 / 6072","12AU7 / ECC82",
    "5751","12DW7 / 7247","6SN7","6DJ8 / ECC88",
    "EF86","6V6","6L6GC","EL84","EL34","KT88"
}};
enum class TubeFlavor : int {
    AX7=0, AT7, AY7, AU7, V5751, DW7, SN7, DJ8, EF86, V6V6,
    V6L6, EL84, EL34, KT88
};

struct TubeSlotControls {
    bool enabled=true;
    int flavor=0;
    float drive=0.30f;
    float bias=0.0f;         // -1..1
    float plate_load=0.50f;
    float sag=0.30f;
    float coupling=0.20f;    // interstage high-pass
    float mix=0.65f;
};

struct TubeRackControls {
    bool enabled=false;     // legacy tube remains exact default for old patches
    bool parallel=false;    // 1->2->3->4 or (1->2) || (3->4)
    float shared_supply=0.35f;
    float output=0.50f;     // 0..1 maps to 0..2 (unity at 0.5)
    bool antialias=true;
    std::array<TubeSlotControls,kTubeRackSlotCount> slots{};
    TubeRackControls() noexcept {
        for(int i=0;i<kTubeRackSlotCount;++i){
            slots[i].flavor=i;
            slots[i].drive=0.32f-0.03f*static_cast<float>(i);
            slots[i].mix=0.65f-0.06f*static_cast<float>(i);
        }
    }
};

class TubeRack final {
public:
    void set_sample_rate(float sample_rate) noexcept;
    void set_controls(const TubeRackControls& controls) noexcept;
    void set_drive_modulation(float amount) noexcept;
    void set_character(float amount) noexcept;
    void reset() noexcept;
    float process(float sample) noexcept;
    const TubeRackControls& controls() const noexcept { return controls_; }
    float supply_envelope() const noexcept { return supply_state_; }
private:
    struct StageState {
        float cathode=0.0f;
        float hp_x=0.0f, hp_y=0.0f, lp_y=0.0f;
        float runtime_drive=0.0f, runtime_bias=0.0f, runtime_mix=0.0f;
        float hp_coeff=0.99f, lp_coeff=0.25f;
        float target_hp=0.99f, target_lp=0.25f;
        float cathode_coeff=0.001f;
        float previous_excited=0.0f;
        bool antialias_ready=false;
        int target_flavor=0,previous_flavor=0;
        float flavor_blend=1.0f,activity=0.0f;
    };
    TubeRackControls controls_{};
    std::array<StageState,kTubeRackSlotCount> states_{};
    float sample_rate_=48000.0f;
    float smooth_coeff_=0.003f,supply_coeff_=0.00025f,supply_state_=0.0f;
    float drive_modulation_=0.0f,character_=0.5f;
    float process_stage(size_t index,float input) noexcept;
    void update_coefficients() noexcept;
};
} // namespace monkeys_ear
