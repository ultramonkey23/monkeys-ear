#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <vector>

namespace monkeys_ear {

// One-pole allpass filter for input diffusion
struct AllpassFilter {
    // Storage is prepared off the audio thread at the maximum supported tap.
    // Changing room size only moves the *read* tap, preserving the tail.
    std::vector<float> buffer;
    size_t write_pos=0;
    float feedback=0.6f;
    float delay_samples=10.0f;
    float target_delay_samples=10.0f;
    float slew_coefficient=1.0f;

    void prepare(size_t capacity, float initial_delay, float fb, float sr) {
        buffer.assign(std::max<size_t>(capacity,16),0.0f);
        write_pos=0;
        feedback=fb;
        delay_samples=target_delay_samples=
            std::clamp(initial_delay,1.0f,static_cast<float>(buffer.size()-2));
        slew_coefficient=1.0f-std::exp(-1.0f/(0.035f*std::max(1000.0f,sr)));
    }
    void set_delay(float delay) noexcept {
        target_delay_samples=std::clamp(delay,1.0f,
            static_cast<float>(buffer.size()-2));
    }
    float process(float x) noexcept {
        if(buffer.empty())return x;
        delay_samples+=(target_delay_samples-delay_samples)*slew_coefficient;
        const float capacity=static_cast<float>(buffer.size());
        float read_pos=static_cast<float>(write_pos)-delay_samples;
        if(read_pos<0.0f)read_pos+=capacity;
        const size_t idx=static_cast<size_t>(read_pos);
        const size_t next=(idx+1)%buffer.size();
        const float delayed=lerp(buffer[idx],buffer[next],
                                 read_pos-static_cast<float>(idx));
        const float vn=x+feedback*delayed;
        const float y=-feedback*vn+delayed;
        buffer[write_pos]=sanitize(vn);
        write_pos=(write_pos+1)%buffer.size();
        return sanitize(y);
    }
    void reset() noexcept {
        std::fill(buffer.begin(),buffer.end(),0.0f);
        write_pos=0;
        delay_samples=target_delay_samples;
    }
};

class FDNReverb {
public:
    static constexpr size_t NUM_DELAYS = 8;

    FDNReverb();
    void set_sample_rate(float sr);
    void set_room_size(float size);    // 0.5 to 2.5 (scales delay lengths)
    void set_decay_time(float t60_s);  // 0.2s to 15.0s (decay time)
    void set_damping(float damp);      // 0.0 to 1.0 (air absorption / high-cut)
    void set_pre_delay(float seconds); // 0.0 to 0.1s
    void set_stereo_width(float width);// 0.0 (mono) to 1.0 (wide)
    void set_mix(float mix);           // 0.0 (dry) to 1.0 (wet)
    void reset();

    void process(float in_l, float in_r, float& out_l, float& out_r);

private:
    float sample_rate_;
    float room_size_;
    float decay_time_;
    float damping_;
    float pre_delay_s_;
    float stereo_width_;
    float mix_;

    // 8 prime delay lines
    std::array<std::vector<float>, NUM_DELAYS> delay_buffers_;
    // Preallocated circular buffers have immutable capacity while processing.
    // Fractional read taps and decay gains slew toward new room/decay settings.
    std::array<size_t, NUM_DELAYS> write_indices_{};
    std::array<float, NUM_DELAYS> current_taps_{};
    std::array<float, NUM_DELAYS> target_taps_{};
    std::array<float, NUM_DELAYS> current_decay_gains_{};
    std::array<float, NUM_DELAYS> target_decay_gains_{};
    float slew_coefficient_=1.0f;
    std::array<float, NUM_DELAYS> damp_states_;

    // Pre-delay buffer
    std::vector<float> pre_delay_buffer_;
    size_t pre_delay_pos_;

    // Input diffusion allpass network (4 cascaded allpass filters)
    std::array<AllpassFilter, 4> input_diffusers_;

    void prepare_buffers(); // permitted allocation: construction/setupProcessing only
    void recalculate(bool snap=false) noexcept; // O(8), no heap or reset
    void apply_hadamard_matrix(std::array<float, NUM_DELAYS>& state) const;
};

} // namespace monkeys_ear
