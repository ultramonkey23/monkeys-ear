#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace monkeys_ear {

// The gestural performance field reuses the existing oscillator/FM, filter,
// ChronoState, Sound Space and drive paths. It adds no signal processor and
// preserves the exact legacy output when both axes are zero.
struct PerformanceField {
    float filter_offset=0.0f;
    float fm_offset=0.0f;
    float drive_offset=0.0f;
    float space_lfo=0.0f;
    float sub_offset=0.0f;
};

inline float field_unit(float v) noexcept {
    // Audio DSP is built with fast-math. Reject NaN/Inf by their IEEE bits.
    if ((std::bit_cast<std::uint32_t>(v)&0x7f800000u)==0x7f800000u)
        return 0.0f;
    return std::clamp(v,0.0f,1.0f);
}
inline float field_bipolar(float v) noexcept {
    if ((std::bit_cast<std::uint32_t>(v)&0x7f800000u)==0x7f800000u)
        return 0.0f;
    return std::clamp(v,-1.0f,1.0f);
}

inline PerformanceField evaluate_performance_field(
    float drift,float fracture,float lfo,float fast_energy,float slow_energy,
    float direction,float velocity,float pressure) noexcept {
    const float x=field_unit(drift),y=field_unit(fracture);
    if(x==0.0f&&y==0.0f)return {};
    const float f=field_unit(fast_energy),s=field_unit(slow_energy);
    const float phase=field_bipolar(lfo);
    const float motion=std::clamp(.66f*phase+.24f*field_bipolar(direction)
                                  +.10f*(f-s),-1.0f,1.0f);
    const float strike=2.0f*field_unit(velocity)-1.0f;
    const float touch=field_unit(pressure);
    const float density=std::clamp(.40f+.35f*std::abs(motion)+.25f*touch,
                                   0.0f,1.0f);
    PerformanceField result{};
    result.filter_offset=x*(.42f*motion+.11f*strike)-y*.07f;
    result.fm_offset=y*(.12f+.14f*x*std::abs(motion)+.04f*touch+.045f*x)
                      +x*.025f*std::abs(phase);
    result.drive_offset=y*(.19f+.22f*x*density);
    result.space_lfo=x*.42f*(.80f*phase+.20f*field_bipolar(direction));
    result.sub_offset=x*.18f*s-y*.07f;
    return result;
}

} // namespace monkeys_ear
