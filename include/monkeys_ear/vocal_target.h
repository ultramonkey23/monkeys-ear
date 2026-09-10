#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <cstdint>

namespace monkeys_ear {

// A tuning degree is a location in a repeating (or non-octave) pitch space.
// Gravity and directional weights describe musical attraction; they are not a
// post-quantizer pitch-bend.  Fixed storage keeps custom tuning LIVE-safe.
struct TuningDegree {
    float cents = 0.0f;
    float gravity = 1.0f;
    float ascending_gravity = 1.0f;
    float descending_gravity = 1.0f;
    bool enabled = true;
    uint32_t metadata = 0;
    std::array<char, 16> label{};
};

enum class VocalBuiltinTuning : uint8_t {
    Chromatic, Major, NaturalMinor, Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    HarmonicMinor, MelodicMinor, PhrygianDominant, LydianSharp2, LydianDominant,
    Altered, MajorPentatonic, MinorPentatonic, Blues, WholeTone, DiminishedHalfWhole,
    DiminishedWholeHalf, Augmented, BebopMajor, BebopDominant, BebopDorian,
    Hirajoshi_0_2_3_7_8, Insen_0_1_5_7_10, Iwato_0_1_5_6_10, Pelog_0_1_3_7_8,
    QuarterTone24, BohlenPierce13, Count
};

struct TuningSpace {
    static constexpr size_t MAX_DEGREES = 48;
    float root_cents = 0.0f;       // offset from A4 = 440 Hz
    float period_cents = 1200.0f;  // may be non-octave
    std::array<TuningDegree, MAX_DEGREES> degrees{};
    uint8_t degree_count = 0;
    VocalBuiltinTuning builtin = VocalBuiltinTuning::Chromatic;
    std::array<char, 32> label{};

    TuningSpace();
    void clear(float period = 1200.0f);
    bool add_degree_cents(float cents, float gravity = 1.0f, float ascending = 1.0f,
                          float descending = 1.0f, const char* degree_label = nullptr);
    bool add_degree_ratio(float numerator, float denominator, float gravity = 1.0f,
                          float ascending = 1.0f, float descending = 1.0f,
                          const char* degree_label = nullptr);
    void set_builtin(VocalBuiltinTuning preset);
    static float ratio_to_cents(float numerator, float denominator);
    static float cents_to_ratio(float cents);
    const char* name() const { return label.data(); }
};

struct VocalTargetControls {
    TuningSpace tuning{};
    // -1 connected/legato through +1 detached/staccato.  This changes history,
    // reacquisition and trajectory shape; it never reduces to correction speed.
    float articulation = 0.0f;
    float portamento = 0.0f;
    float transition_preservation = 0.5f;
    float target_hysteresis = 0.45f;
    float onset_protection = 0.5f;
    float directionality = 1.0f;
};

struct VocalTargetMetrics {
    float selected_cents = 0.0f;
    float trajectory_cents = 0.0f;
    float motion_cents_per_second = 0.0f;
    float selection_score = 0.0f;
    int selected_degree = -1;
    uint32_t switch_count = 0;
    bool switched = false;
};

class VocalTargetEngine {
public:
    void set_sample_rate(float sample_rate);
    void set_controls(const VocalTargetControls& controls) { controls_ = controls; }
    void reset();
    // Causal target generation from pitch evidence.  onset is a 0..1 evidence
    // value, not amplitude shaping; callers may automate or supply MIDI onset.
    float process(float observed_cents, float motion_cents_per_second,
                  float confidence, float onset);
    const VocalTargetMetrics& metrics() const { return metrics_; }

private:
    float candidate_score(float candidate, const TuningDegree& degree,
                          float observed, float motion) const;
    float sample_rate_ = 48000.0f;
    VocalTargetControls controls_{};
    VocalTargetMetrics metrics_{};
    float previous_target_ = 0.0f;
    float trajectory_ = 0.0f;
    int previous_degree_ = -1;
    bool has_target_ = false;
};

} // namespace monkeys_ear
