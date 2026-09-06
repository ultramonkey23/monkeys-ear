#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <vector>

namespace monkeys_ear {

// One-pole allpass filter for input diffusion
struct AllpassFilter {
    std::vector<float> buffer;
    size_t write_pos = 0;
    float feedback = 0.5f;

    void init(size_t delay_samples, float fb = 0.5f) {
        buffer.resize(delay_samples, 0.0f);
        write_pos = 0;
        feedback = fb;
    }

    float process(float x) {
        if (buffer.empty()) return x;
        float buf_out = buffer[write_pos];
        float vn = x + feedback * buf_out;
        float y = -feedback * vn + buf_out;
        buffer[write_pos] = vn;
        write_pos = (write_pos + 1) % buffer.size();
        return sanitize(y);
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        write_pos = 0;
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
    std::array<size_t, NUM_DELAYS> delay_lengths_;
    std::array<size_t, NUM_DELAYS> write_indices_;
    std::array<float, NUM_DELAYS> decay_gains_;
    std::array<float, NUM_DELAYS> damp_states_;

    // Pre-delay buffer
    std::vector<float> pre_delay_buffer_;
    size_t pre_delay_pos_;

    // Input diffusion allpass network (4 cascaded allpass filters)
    std::array<AllpassFilter, 4> input_diffusers_;

    void recalculate();
    void apply_hadamard_matrix(std::array<float, NUM_DELAYS>& state) const;
};

} // namespace monkeys_ear
