#pragma once

#include "monkeys_ear/types.h"
#include <vector>

namespace monkeys_ear {

class StereoDelay {
public:
    static constexpr size_t MAX_DELAY_SAMPLES = 192000; // 2 seconds @ 96kHz or 4s @ 48kHz

    StereoDelay();
    void set_sample_rate(float sr);
    void set_time_left(float seconds);
    void set_time_right(float seconds);
    void set_feedback(float fb);       // 0.0 to 0.95
    void set_cross_feedback(float xfb);// 0.0 (parallel) to 1.0 (ping-pong)
    void set_high_damping(float damp); // 0.0 to 1.0 (analog tape high-cut)
    void set_mix(float mix);           // 0.0 (dry) to 1.0 (wet)
    void reset();

    void process(float in_l, float in_r, float& out_l, float& out_r);

private:
    float sample_rate_;
    float time_l_;
    float time_r_;
    float feedback_;
    float cross_feedback_;
    float high_damping_;
    float mix_;

    std::vector<float> buffer_l_;
    std::vector<float> buffer_r_;
    size_t write_pos_;

    // Lowpass filter states in feedback path
    float damp_l_;
    float damp_r_;

    float read_interpolated(const std::vector<float>& buf, float delay_samples) const;
};

} // namespace monkeys_ear
