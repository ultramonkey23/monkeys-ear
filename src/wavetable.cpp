#include "monkeys_ear/wavetable.h"
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

namespace {

inline float apply_fold_inline(float x, float fold01) noexcept {
    if (fold01 <= 0.001f) {
        return x;
    }
    const float drive = 1.0f + fold01 * 3.1f;
    const float folded = std::sin(x * drive * 1.57079632679f);
    const float grit = fast_tanh(x * (1.2f + fold01 * 2.0f));
    const float blended = lerp(folded, grit, 0.25f);
    // Keep perceptual loudness steady even at heavy wavefold
    return lerp(x, blended * 0.92f, clamp(fold01 * 1.15f, 0.0f, 1.0f));
}

inline void build_generator_spectrum(const WaveformGeneratorParams& p,
                                     std::array<float, WaveformBank::kNumHarmonics>& amps,
                                     std::array<float, WaveformBank::kNumHarmonics>& phases) noexcept {
    const float shape = clamp(p.shape, 0.0f, 1.0f);
    const float odd_even = clamp(p.odd_even, 0.0f, 1.0f);
    const float tilt = clamp(p.tilt, 0.0f, 1.0f);
    const float formant_h = lerp(1.5f, 18.0f, clamp(p.formant, 0.0f, 1.0f));
    const float slope_exp = lerp(1.75f, 0.42f, tilt);

    for (size_t i = 0; i < WaveformBank::kNumHarmonics; ++i) {
        const float h = static_cast<float>(i + 1);
        const bool is_odd = ((i + 1) & 1u) != 0u;

        // 1. Base rolloff curve controlled by spectral tilt
        float base = 1.0f / std::pow(h, slope_exp);

        // 2. Odd vs Even harmonic weighting
        float parity = 1.0f;
        if (is_odd) {
            parity = lerp(0.18f, 1.35f, odd_even);
        } else {
            parity = lerp(1.35f, 0.14f, odd_even);
        }
        if (i == 0) {
            parity = std::max(0.75f, parity); // protect fundamental anchor
        }

        // 3. Archetype shape character across spectrum
        float archetype = 1.0f;
        if (shape < 0.33f) {
            // Smooth -> Reed
            float t = shape / 0.33f;
            float reed = (i % 3 == 0) ? 1.25f : 0.7f;
            archetype = lerp(1.0f, reed, t);
        } else if (shape < 0.67f) {
            // Reed -> Sync-style spectral comb
            float t = (shape - 0.33f) / 0.34f;
            float comb = 0.55f + 0.45f * std::cos(h * (0.75f + 0.9f * t));
            archetype = lerp(0.85f, comb * 1.35f, t);
        } else {
            // Sync Comb -> Metallic Bell partials
            float t = (shape - 0.67f) / 0.33f;
            float bell = (i == 0 || i == 2 || i == 4 || i == 6 || i == 10 || i == 14) ? 1.45f : 0.25f;
            archetype = lerp(1.0f, bell, t);
        }

        // 4. Formant peak boost (Gaussian spectral hill around formant_h)
        const float dist = (h - formant_h) / std::max(1.2f, formant_h * 0.45f);
        const float formant_boost = 1.0f + (1.45f * std::exp(-0.5f * dist * dist));

        amps[i] = base * parity * archetype * formant_boost;
        phases[i] = shape * 0.45f * std::sin(h * 0.85f);
    }
}

} // namespace

WaveformBank::WaveformBank() noexcept {
    build_static_families();
    WaveformGeneratorParams def{};
    regenerate_generator(def);
}

void WaveformBank::build_from_harmonics(MipTable& target,
                                        const std::array<float, kNumHarmonics>& amps,
                                        const std::array<float, kNumHarmonics>& phases) noexcept {
    constexpr int max_harms[kMipCount] = { 32, 20, 12, 7, 4, 2, 1 };

    for (size_t mip = 0; mip < kMipCount; ++mip) {
        const int cap = std::min(static_cast<int>(kNumHarmonics), max_harms[mip]);
        float peak = 0.0f;
        for (size_t s = 0; s < kTableSize; ++s) {
            const float ph = static_cast<float>(s) / static_cast<float>(kTableSize);
            float sum = 0.0f;
            for (int h = 0; h < cap; ++h) {
                const float angle = TWO_PI * static_cast<float>(h + 1) * ph + phases[h];
                sum += amps[h] * std::sin(angle);
            }
            target.mips[mip][s] = sum;
            peak = std::max(peak, std::abs(sum));
        }
        const float norm = (peak > 1e-5f) ? (0.94f / peak) : 1.0f;
        for (size_t s = 0; s < kTableSize; ++s) {
            target.mips[mip][s] *= norm;
        }
    }
}

void WaveformBank::build_static_families() noexcept {
    std::array<float, kNumHarmonics> amps{};
    std::array<float, kNumHarmonics> phases{};

    // 0: Classic Saw
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        amps[i] = 1.0f / static_cast<float>(i + 1);
        phases[i] = 0.0f;
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Saw)], amps, phases);

    // 1: Hollow Pulse / Square
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        amps[i] = ((i % 2) == 0) ? (1.0f / static_cast<float>(i + 1)) : 0.0f;
        phases[i] = 0.0f;
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Pulse)], amps, phases);

    // 2: Warm Triangle
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        const float h = static_cast<float>(i + 1);
        if ((i % 2) == 0) {
            const float sign = ((i / 2) % 2 == 0) ? 1.0f : -1.0f;
            amps[i] = sign / (h * h);
        } else {
            amps[i] = 0.0f;
        }
        phases[i] = 0.0f;
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Triangle)], amps, phases);

    // 3: Pure Sine (+ tiny warm 2nd harmonic option stays clean)
    amps.fill(0.0f);
    phases.fill(0.0f);
    amps[0] = 1.0f;
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Sine)], amps, phases);

    // 4: Formant (vocal twin peaks around harmonics 3 and 7)
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        const float h = static_cast<float>(i + 1);
        const float f1 = std::exp(-0.5f * ((h - 3.2f) / 1.3f) * ((h - 3.2f) / 1.3f));
        const float f2 = 0.72f * std::exp(-0.5f * ((h - 7.5f) / 1.9f) * ((h - 7.5f) / 1.9f));
        amps[i] = (0.35f / std::pow(h, 0.85f)) + 0.85f * f1 + 0.65f * f2;
        phases[i] = 0.12f * h;
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Formant)], amps, phases);

    // 5: BrassFold (bold mid harmonics + punchy fundamental + edge)
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        const float h = static_cast<float>(i + 1);
        const float brass = (h <= 6.0f) ? (1.0f / std::pow(h, 0.55f)) : (0.45f / std::pow(h, 1.15f));
        amps[i] = brass * (1.0f + 0.25f * std::sin(h * 0.9f));
        phases[i] = (i > 0) ? 0.4f : 0.0f;
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::BrassFold)], amps, phases);

    // 6: Organ (drawbar register stack: 1, 2, 3, 4, 6, 8, 12, 16)
    amps.fill(0.0f);
    phases.fill(0.0f);
    amps[0] = 0.95f;
    amps[1] = 0.82f;
    amps[2] = 0.55f;
    amps[3] = 0.72f;
    amps[5] = 0.42f;
    amps[7] = 0.56f;
    amps[11] = 0.24f;
    amps[15] = 0.20f;
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::Organ)], amps, phases);

    // 7: GlassBell (crystal odd cluster + high partial sparkle)
    amps.fill(0.0f);
    phases.fill(0.0f);
    amps[0] = 0.90f;
    amps[2] = 0.55f;
    amps[5] = 0.48f;
    amps[6] = 0.40f;
    amps[10] = 0.34f;
    amps[13] = 0.26f;
    amps[18] = 0.18f;
    amps[23] = 0.12f;
    for (size_t i = 0; i < kNumHarmonics; ++i) {
        phases[i] = 0.65f * static_cast<float>(i & 3u);
    }
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::GlassBell)], amps, phases);
}

void WaveformBank::regenerate_generator(const WaveformGeneratorParams& params) noexcept {
    if (gen_initialized_ && last_gen_.matches(params)) {
        return;
    }
    std::array<float, kNumHarmonics> amps{};
    std::array<float, kNumHarmonics> phases{};
    build_generator_spectrum(params, amps, phases);
    build_from_harmonics(tables_[static_cast<size_t>(WaveFamily::CustomGen)], amps, phases);
    last_gen_ = params;
    gen_initialized_ = true;
}

float WaveformBank::warp_phase(float phase01, float warp01) noexcept {
    float p = phase01 - std::floor(phase01);
    const float w = clamp(warp01, 0.08f, 0.92f);
    if (std::abs(w - 0.5f) < 1e-4f) {
        return p;
    }
    // Smooth two-segment Casio/CZ style phase distortion without step jump
    if (p < w) {
        return 0.5f * (p / w);
    }
    return 0.5f + 0.5f * ((p - w) / (1.0f - w));
}

float WaveformBank::lookup_mip(const MipTable& table, float phase01, float dt) const noexcept {
    float p = phase01 - std::floor(phase01);
    if (p < 0.0f) p += 1.0f;

    // Choose mip level by Nyquist harmonic limit: max_h <= 0.45 / dt
    const float safe_dt = std::max(1e-5f, std::abs(dt));
    const float nyquist_harms = 0.42f / safe_dt;
    // Map [32 .. 1] to mip [0 .. 6]
    float mip_f = 0.0f;
    if (nyquist_harms < 32.0f) {
        mip_f = clamp(5.0f - std::log2(std::max(1.0f, nyquist_harms)), 0.0f, static_cast<float>(kMipCount - 1));
    }

    const size_t m0 = static_cast<size_t>(mip_f);
    const size_t m1 = std::min(kMipCount - 1, m0 + 1);
    const float m_frac = mip_f - static_cast<float>(m0);

    const float pos = p * static_cast<float>(kTableSize);
    const size_t idx0 = static_cast<size_t>(pos) & (kTableSize - 1);
    const size_t idx1 = (idx0 + 1) & (kTableSize - 1);
    const float s_frac = pos - std::floor(pos);

    const float v0 = lerp(table.mips[m0][idx0], table.mips[m0][idx1], s_frac);
    if (m0 == m1) {
        return v0;
    }
    const float v1 = lerp(table.mips[m1][idx0], table.mips[m1][idx1], s_frac);
    return lerp(v0, v1, m_frac);
}

float WaveformBank::sample_family(WaveFamily family, float phase01, float dt) const noexcept {
    const int idx = clamp(static_cast<int>(family), 0, kWaveFamilyCount - 1);
    return lookup_mip(tables_[static_cast<size_t>(idx)], phase01, dt);
}

float WaveformBank::sample_morph(WaveFamily family_a,
                                 WaveFamily family_b,
                                 float morph01,
                                 float phase01,
                                 float dt,
                                 float phase_warp01,
                                 float wave_fold01) const noexcept {
    const float warped = warp_phase(phase01, phase_warp01);
    const float m = clamp(morph01, 0.0f, 1.0f);
    const float a = sample_family(family_a, warped, dt);
    float raw = a;
    if (m > 1e-4f) {
        const float b = sample_family(family_b, warped, dt);
        raw = lerp(a, b, m);
    }
    return apply_fold_inline(raw, wave_fold01);
}

void WaveformBank::render_preview(WaveFamily family_a,
                                  WaveFamily family_b,
                                  float morph01,
                                  const WaveformGeneratorParams& gen,
                                  std::array<float, 128>& out_samples) noexcept {
    thread_local WaveformBank ui_bank;
    ui_bank.regenerate_generator(gen);
    constexpr float kPreviewDt = 1.0f / 128.0f;
    for (size_t i = 0; i < out_samples.size(); ++i) {
        const float ph = static_cast<float>(i) / static_cast<float>(out_samples.size());
        out_samples[i] = ui_bank.sample_morph(family_a, family_b, morph01, ph, kPreviewDt, gen.phase_warp, gen.wave_fold);
    }
}

const WaveformBank& shared_waveform_bank() noexcept {
    static const WaveformBank bank{};
    return bank;
}

} // namespace monkeys_ear
