#include "monkeys_ear/tonal.h"

#include <cmath>
#include <iostream>
#include <utility>

using namespace monkeys_ear;

namespace {
constexpr float kSampleRate = 48000.0f;
constexpr float kPi = 3.14159265358979323846f;

struct TrackResult {
    float hz = 0.0f;
    float confidence = 0.0f;
};

TrackResult track_case(float f0_gain, float second_gain) {
    ExternalSubharmonic tracker;
    tracker.set_sample_rate(kSampleRate);
    tracker.set_ratio(2);

    constexpr float f0 = 110.0f;
    for (size_t i = 0; i < 48000; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        const float x = f0_gain * std::sin(2.0f * kPi * f0 * t)
                      + second_gain * std::sin(2.0f * kPi * 2.0f * f0 * t);
        tracker.process(x);
    }
    return {tracker.tracked_frequency_hz(), tracker.confidence()};
}

bool in_tracker_range(float hz) {
    return std::isfinite(hz) && hz >= 55.0f && hz <= 900.0f;
}
}

int main() {
    const auto clean = track_case(0.60f, 0.00f);
    const auto strong_second = track_case(0.16f, 0.70f);
    const auto missing_fundamental = track_case(0.00f, 0.70f);

    std::cout << "Vocal tracker characterization\n"
              << "  clean F0:          " << clean.hz << " Hz, confidence " << clean.confidence << "\n"
              << "  strong 2nd:        " << strong_second.hz << " Hz, confidence " << strong_second.confidence << "\n"
              << "  missing F0 / 2nd:  " << missing_fundamental.hz << " Hz, confidence " << missing_fundamental.confidence << "\n";

    if (std::abs(clean.hz - 110.0f) >= 1.0f || clean.confidence <= 0.75f) {
        std::cerr << "FAIL: clean 110 Hz baseline regressed\n";
        return 1;
    }
    if (!in_tracker_range(strong_second.hz) || !in_tracker_range(missing_fundamental.hz)) {
        std::cerr << "FAIL: adversarial case produced invalid tracker state\n";
        return 1;
    }

    // Deliberately characterize rather than require the correct octave yet.
    // A later arbitration change can tighten these expectations after evidence.
    const bool strong_second_octave = std::abs(strong_second.hz - 220.0f) < 2.0f;
    const bool missing_f0_octave = std::abs(missing_fundamental.hz - 220.0f) < 2.0f;
    std::cout << "  octave flags: strong2=" << strong_second_octave
              << ", missingF0=" << missing_f0_octave << "\n";
    return 0;
}
