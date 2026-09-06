#pragma once

#include "monkeys_ear/types.h"

namespace monkeys_ear {

class TubeDriveStage {
public:
    TubeDriveStage();
    void set_sample_rate(float sr);
    void set_drive(float drive);   // 0.0 (clean) to 1.0 (screaming saturation)
    void set_bias(float bias);     // -1.0 to 1.0 (asymmetry / even harmonics)
    void set_memory_sag(float sag);// 0.0 to 1.0 (dynamic thermal / cathode sag)
    void set_mix(float mix);       // 0.0 (dry) to 1.0 (wet)
    void reset();

    float process(float input);

private:
    float sample_rate_;
    float drive_;
    float bias_;
    float memory_sag_;
    float mix_;

    // Dynamic state: cathode bias tracker (thermal / capacitor charge)
    float cathode_charge_;
    float sag_coeff_;

    void recalculate();
};

} // namespace monkeys_ear
