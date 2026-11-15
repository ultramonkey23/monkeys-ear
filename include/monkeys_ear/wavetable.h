#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <cstdint>

namespace monkeys_ear {

enum class WaveFamily : int {
    Saw = 0,
    Pulse = 1,
    Triangle = 2,
    Sine = 3,
    Formant = 4,
    BrassFold = 5,
    Organ = 6,
    GlassBell = 7,
    CustomGen = 8,
    Count = 9
};

constexpr int kWaveFamilyCount = static_cast<int>(WaveFamily::Count);

inline constexpr std::array<const char*, kWaveFamilyCount> kWaveFamilyNames{{
    "Saw", "Pulse", "Triangle", "Sine",
    "Formant", "BrassFold", "Organ", "GlassBell", "Generator"
}};

// Shared parametric controls for the Custom Waveform Generator.
// All fields are normalized [0, 1] and real-time safe.
struct WaveformGeneratorParams {
    float shape = 0.25f;       // Core spectral archetype blend (Warm -> Nasal -> Sync -> Reed)
    float odd_even = 0.50f;    // Odd vs Even harmonic emphasis (0 = even, 0.5 = balanced, 1 = odd)
    float tilt = 0.55f;        // Spectral brightness / harmonic rolloff
    float formant = 0.35f;     // Formant resonance peak center
    float phase_warp = 0.50f;  // Phase distortion symmetry
    float wave_fold = 0.0f;    // Chebyshev / sine wavefold intensity

    bool matches(const WaveformGeneratorParams& o) const noexcept {
        return std::abs(shape - o.shape) < 1e-4f &&
               std::abs(odd_even - o.odd_even) < 1e-4f &&
               std::abs(tilt - o.tilt) < 1e-4f &&
               std::abs(formant - o.formant) < 1e-4f;
    }
};

// Band-limited mipmapped wavetable storage & real-time harmonic generator.
// Zero heap allocations; all frames live inside fixed-size std::arrays.
class WaveformBank {
public:
    static constexpr size_t kTableSize = 512;
    static constexpr size_t kMipCount = 7; // max harmonics: 64, 32, 16, 8, 4, 2, 1
    static constexpr size_t kNumHarmonics = 32;

    WaveformBank() noexcept;

    void regenerate_generator(const WaveformGeneratorParams& params) noexcept;

    // Sample a specific wave family at phase [0, 1) and phase increment dt (cycles/sample).
    float sample_family(WaveFamily family, float phase01, float dt) const noexcept;

    // Morph smoothly between family_a and family_b (t in [0, 1]) with optional phase warp & fold.
    float sample_morph(WaveFamily family_a,
                       WaveFamily family_b,
                       float morph01,
                       float phase01,
                       float dt,
                       float phase_warp01 = 0.5f,
                       float wave_fold01 = 0.0f) const noexcept;

    // Render 128 preview samples of the morphed + warped + folded waveform for UI visualization.
    static void render_preview(WaveFamily family_a,
                               WaveFamily family_b,
                               float morph01,
                               const WaveformGeneratorParams& gen,
                               std::array<float, 128>& out_samples) noexcept;

private:
    struct MipTable {
        std::array<std::array<float, kTableSize>, kMipCount> mips{};
    };

    std::array<MipTable, kWaveFamilyCount> tables_{};
    WaveformGeneratorParams last_gen_{};
    bool gen_initialized_ = false;

    void build_static_families() noexcept;
    void build_from_harmonics(MipTable& target,
                              const std::array<float, kNumHarmonics>& amps,
                              const std::array<float, kNumHarmonics>& phases) noexcept;
    float lookup_mip(const MipTable& table, float phase01, float dt) const noexcept;
    static float warp_phase(float phase01, float warp01) noexcept;
};

// Shared static bank initialized once for static families; per-voice/engine owns its generator slot cleanly.
const WaveformBank& shared_waveform_bank() noexcept;

} // namespace monkeys_ear
