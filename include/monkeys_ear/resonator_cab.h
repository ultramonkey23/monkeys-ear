#pragma once

#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

// Second-order modal resonator filter
struct ModalPole {
    float freq_hz;
    float decay_s;
    float gain;
    float y1;
    float y2;
    float a1;
    float a2;
    float b0;

    void update(float sample_rate) {
        float omega = TWO_PI * freq_hz / sample_rate;
        float r = std::exp(-1.0f / (decay_s * sample_rate));
        a1 = -2.0f * r * std::cos(omega);
        a2 = r * r;
        b0 = (1.0f - r * r) * 0.5f;
    }

    float process(float x) {
        float y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return sanitize(y);
    }

    void reset() {
        y1 = 0.0f;
        y2 = 0.0f;
    }
};

class CabinetResonator {
public:
    CabinetResonator();
    void set_sample_rate(float sr);
    void set_body_size(float size);   // 0.5 (small/tight) to 2.0 (large/deep)
    void set_resonance(float res);   // 0.0 to 1.0 (Q / ringing decay)
    void set_damping(float damping); // 0.0 to 1.0 (high frequency air loss)
    void set_mix(float mix);         // 0.0 (dry) to 1.0 (wet)
    void reset();

    float process(float input);

private:
    float sample_rate_;
    float body_size_;
    float resonance_;
    float damping_;
    float mix_;

    std::array<ModalPole, 4> poles_;
    void update_poles();
};

} // namespace monkeys_ear
