#pragma once

#include "monkeys_ear/types.h"

namespace monkeys_ear {

enum class FilterMode {
    Lowpass,
    Bandpass,
    Highpass,
    Notch
};

class StateVariableFilter {
public:
    StateVariableFilter();
    void set_sample_rate(float sr);
    void set_cutoff(float freq_hz);
    void set_resonance(float res); // 0.0 to 1.0 (maps to Q ~ 0.5 to 25.0)
    void set_mode(FilterMode mode);
    void set_drive(float drive);   // Internal filter drive/saturation
    void reset();

    float process(float input, float env_mod = 0.0f, float env_amount = 0.0f);

private:
    float sample_rate_;
    float cutoff_hz_;
    float resonance_;
    FilterMode mode_;
    float drive_;

    // Internal state variables (trapezoidal integration)
    float s1_;
    float s2_;

    // Coefficients
    float g_;
    float k_;

    void update_coefficients();
};

} // namespace monkeys_ear
