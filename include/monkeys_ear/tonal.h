#pragma once

#include "monkeys_ear/filter.h"
#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

enum class FilterRouting { Serial = 0, Parallel, Split };

class MultiPassFilter {
public:
    void set_sample_rate(float sr);
    void reset();
    void set_stage(int stage, FilterMode mode, float cutoff_hz, float resonance,
                   float drive, bool slope_24db);
    void set_routing(FilterRouting routing) { routing_ = routing; }
    void set_mix(float mix) { mix_ = clamp(mix, 0.0f, 1.0f); }
    float process(float input, float modulation = 0.0f, float modulation_depth = 0.0f);

private:
    std::array<StateVariableFilter, 4> filters_{};
    std::array<bool, 2> slope_24db_{{false, false}};
    FilterRouting routing_ = FilterRouting::Serial;
    float mix_ = 1.0f;
};

enum class EQType { Bypass = 0, Bell, LowShelf, HighShelf, Highpass, Lowpass };

struct EQBandSettings {
    EQType type = EQType::Bypass;
    float frequency_hz = 1000.0f;
    float gain_db = 0.0f;
    float q = 0.707f;
};

class ParametricEQ {
public:
    static constexpr size_t NUM_BANDS = 4;
    void set_sample_rate(float sr);
    void reset();
    void set_band(size_t band, const EQBandSettings& settings);
    void set_bypass(bool bypass) { bypass_ = bypass; }
    void set_gain_compensation(bool enabled) { gain_compensation_ = enabled; }
    float process(float input, float frequency_mod_octaves = 0.0f,
                  float gain_mod_db = 0.0f);
    const EQBandSettings& get_band(size_t band) const { return settings_[band]; }

private:
    struct Coefficients { float b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct BandState { Coefficients current{}, target{}; float z1=0, z2=0; };
    float sample_rate_ = 48000.0f;
    std::array<EQBandSettings, NUM_BANDS> settings_{};
    std::array<BandState, NUM_BANDS> bands_{};
    bool bypass_ = false;
    bool gain_compensation_ = true;
    float compensation_current_ = 1.0f;
    unsigned modulation_counter_ = 0;
    void update_band(size_t band, float frequency_mod_octaves, float gain_mod_db);
};

// Causal, no-lookahead external-audio divider. It accepts stable monophonic
// fundamentals from 55-900 Hz. Two positive crossings are required (roughly
// 1-36 ms); ambiguous/polyphonic/noisy input fades the generated sub to zero.
class ExternalSubharmonic {
public:
    void set_sample_rate(float sr);
    void reset();
    void set_ratio(int denominator) { ratio_denominator_ = std::clamp(denominator, 1, 4); }
    void set_phase(float phase_cycles) { phase_offset_ = phase_cycles - std::floor(phase_cycles); }
    void set_polarity(bool inverted) { polarity_ = inverted ? -1.0f : 1.0f; }
    void set_saturation(float amount) { saturation_ = clamp(amount, 0.0f, 1.0f); }
    float process(float input);
    float tracked_frequency_hz() const { return tracked_frequency_hz_; }
    float confidence() const { return confidence_; }

private:
    float sample_rate_ = 48000.0f;
    float previous_input_ = 0.0f;
    int samples_since_crossing_ = 0;
    int previous_period_ = 0;
    int stale_samples_ = 0;
    int ratio_denominator_ = 2;
    float phase_ = 0.0f;
    float phase_offset_ = 0.0f;
    float polarity_ = 1.0f;
    float saturation_ = 0.0f;
    float tracked_frequency_hz_ = 0.0f;
    float confidence_ = 0.0f;
    float envelope_ = 0.0f;
};

class MotionSmoother {
public:
    void set_sample_rate(float sr, float time_ms = 8.0f);
    void reset(float value = 0.0f) { value_ = value; }
    float process(float target);
private:
    float coefficient_ = 0.0f;
    float value_ = 0.0f;
};

inline float shape_modulation(float value, float curve, bool bipolar = true) {
    float v = bipolar ? clamp(value, -1.0f, 1.0f) : clamp(value, 0.0f, 1.0f);
    float exponent = 0.35f + clamp(curve, 0.0f, 1.0f) * 2.65f;
    if (!bipolar) return std::pow(v, exponent);
    return std::copysign(std::pow(std::abs(v), exponent), v);
}

} // namespace monkeys_ear
