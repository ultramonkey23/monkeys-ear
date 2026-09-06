#pragma once

#include "monkeys_ear/types.h"

namespace monkeys_ear {

class SafetyLimiter {
public:
    SafetyLimiter();
    void set_ceiling(float ceiling_db); // e.g. -0.2 dBFS
    void reset();

    // Process stereo frame in-place with zero latency
    void process(float& left, float& right);

private:
    float ceiling_linear_;
    float threshold_;
};

} // namespace monkeys_ear
