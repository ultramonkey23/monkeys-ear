#pragma once

#include "monkeys_ear/types.h"
#include <cmath>

namespace monkeys_ear {

enum class LFOWaveform {
    Sine = 0,
    Triangle,
    Saw,
    Square,
    SampleAndHold
};

class LFO {
public:
    LFO();
    void set_sample_rate(float sr);
    void set_rate_hz(float rate_hz); // 0.05 Hz to 30.0 Hz
    void set_depth(float depth);     // 0.0 to 1.0
    void set_waveform(LFOWaveform wf);
    void reset_phase(float phase = 0.0f);

    float process();
    float get_current_value() const { return current_value_; }

private:
    float sample_rate_;
    float rate_hz_;
    float depth_;
    LFOWaveform waveform_;

    float phase_;
    float phase_increment_;
    float current_value_;
    float last_sh_val_;
    uint32_t rng_state_;

    void update_increment();
    float next_random();
};

} // namespace monkeys_ear
