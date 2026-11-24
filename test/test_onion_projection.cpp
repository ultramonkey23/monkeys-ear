#ifdef NDEBUG
#undef NDEBUG
#endif
#include "monkeys_ear/onion_projection.h"
#include "monkeys_ear/onion_skin_eq.h"
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <random>

using namespace monkeys_ear::onion_projection;

void test_onion_projection() {
    Bands self{}, ref{}, proposal{};
    self[0] = ref[0] = 1.0f;
    proposal[0] = 1.2f;
    const auto boost_only = select(self, ref, proposal);
    assert(boost_only.gain_db[0] == 0.0f);
    assert(boost_only.conflict_before == boost_only.conflict_after);
    assert(boost_only.identity == 1.0f);

    self[12] = ref[12] = 0.1f;
    proposal[12] = -0.5f;
    const auto alternative = select(self, ref, proposal);
    assert(alternative.gain_db[0] == 0.0f);
    assert(alternative.gain_db[12] < 0.0f);
    assert(alternative.conflict_after < alternative.conflict_before);

    proposal[0] = 0.1f;
    self[12] = ref[12] = 0.9f;
    proposal[12] = -3.0f;
    const auto preserve = select(self, ref, proposal);
    assert(preserve.gain_db[0] > 0.0f);
    assert(preserve.gain_db[12] < 0.0f);

    // Verify the returned gains and metrics are the SAME evaluated candidate.
    std::mt19937 rng(910);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    int accepted = 0;
    for (int trial = 0; trial < 4000; ++trial) {
        for (std::size_t b = 0; b < kBands; ++b) {
            self[b] = unit(rng); ref[b] = unit(rng);
            proposal[b] = 14.0f * unit(rng) - 12.0f;
            if (trial % 7 == 0) ref[b] = 0.0f;
        }
        const auto d = select(self, ref, proposal);
        float before = 0.0f, after = 0.0f, dot = 0.0f, n0 = 0.0f, n1 = 0.0f;
        bool changed = false;
        for (std::size_t b = 0; b < kBands; ++b) {
            const float db = d.gain_db[b];
            changed |= db != 0.0f;
            assert(std::abs(db) <= std::abs(proposal[b]) + 1e-6f);
            assert(db * proposal[b] >= 0.0f);
            const float projected = self[b] * std::pow(10.0f, db * 0.05f);
            before += std::sqrt(self[b] * ref[b]);
            after += std::sqrt(projected * ref[b]);
            dot += self[b] * projected;
            n0 += self[b] * self[b]; n1 += projected * projected;
        }
        assert(std::abs(d.conflict_before - before) < 1e-4f);
        assert(std::abs(d.conflict_after - after) < 1e-4f);
        assert(after <= before + 1e-5f);
        assert(d.identity >= 0.86f);
        assert(std::abs(d.identity - dot / std::sqrt(n0 * n1)) < 1e-5f);
        if (changed) { assert(after < before); ++accepted; }
    }
    assert(accepted > 2000);
    std::cout << "Onion projection: " << accepted << "/4000 non-neutral accepted; all bounds passed\n";
}

// Independent transfer-function evaluation of the normalized RBJ cascade.
static double cascade_amplitude(const Bands& gains, float sr, float q, std::size_t band) {
    const auto& centers = monkeys_ear::OnionSkinEQ::kBandCentersHz;
    constexpr double two_pi = 6.28318530717958647692;
    const double w = two_pi * std::clamp(centers[band], 30.0f, sr * 0.44f) / sr;
    const auto z = std::polar(1.0, -w);
    double magnitude = 1.0;
    for (std::size_t j = 0; j < kBands; ++j) {
        const double w0 = two_pi * std::clamp(centers[j], 30.0f, sr * 0.44f) / sr;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double A = std::pow(10.0, gains[j] / 40.0);
        const double c = -2.0 * std::cos(w0);
        const auto num = (1.0 + alpha * A) + c * z + (1.0 - alpha * A) * z * z;
        const auto den = (1.0 + alpha / A) + c * z + (1.0 - alpha / A) * z * z;
        magnitude *= std::abs(num / den);
    }
    return magnitude;
}

static void test_cascade_projection() {
    std::mt19937 rng(914);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const auto& centers = monkeys_ear::OnionSkinEQ::kBandCentersHz;
    int accepted = 0;
    for (float sr : {32000.0f, 48000.0f, 96000.0f}) {
        for (float q : {0.85f, 1.85f, 4.5f}) {
            FilterGeometry g;
            g.configure(sr, q, centers);
            Bands self{}, ref{}, proposal{}, guard{};
            for (int trial = 0; trial < 300; ++trial) {
                for (std::size_t b = 0; b < kBands; ++b) {
                    self[b] = unit(rng); ref[b] = unit(rng);
                    proposal[b] = trial == 0 ? -6.0f : 14.0f * unit(rng) - 12.0f;
                    guard[b] = trial % 2 && b == 5 ? 0.85f : 0.0f;
                }
                const auto d = select(self, ref, proposal, &g, &guard);
                const float cut = std::min(0.0f, *std::min_element(proposal.begin(), proposal.end()));
                const float boost = std::max(0.0f, *std::max_element(proposal.begin(), proposal.end()));
                double before = 0.0, after = 0.0, dot = 0.0, n0 = 0.0, n1 = 0.0;
                bool changed = false;
                for (std::size_t b = 0; b < kBands; ++b) {
                    changed |= d.gain_db[b] != 0.0f;
                    assert(std::abs(d.gain_db[b]) <= std::abs(proposal[b]) + 1e-6f);
                    const double amp = cascade_amplitude(d.gain_db, sr, q, b);
                    const double db = 20.0 * std::log10(amp);
                    assert(db >= cut * (1.0f - guard[b]) - 0.025);
                    assert(db <= boost + 0.025);
                    const double projected = self[b] * amp;
                    before += std::sqrt(self[b] * ref[b]);
                    after += std::sqrt(projected * ref[b]);
                    dot += self[b] * projected;
                    n0 += self[b] * self[b]; n1 += projected * projected;
                }
                assert(after <= before + 1e-5);
                assert(dot / std::sqrt(n0 * n1) >= 0.8599);
                assert(std::abs(d.conflict_after - after) < 0.005);
                if (changed) ++accepted;
                if (trial == 0) assert(changed); // broadband is not reduced to bypass
            }
        }
    }
    assert(accepted > 2400);
    std::cout << "Cascade projection: " << accepted << "/2700 accepted; independent response bounds passed\n";
}

static void test_local_harmonic_protection() {
    using namespace monkeys_ear;
    for (float q : {0.85f, 1.85f, 4.5f}) {
        std::array<float, 2> measured{};
        for (int guard = 0; guard < 2; ++guard) {
            OnionSkinEQ eq;
            OnionSkinControls c;
            c.enabled = true; c.amount = 1.0f; c.sensitivity = 1.0f;
            c.low_protect = 0.0f; c.transient_protect = 0.0f;
            c.harmonic_protect = float(guard); c.q_focus = q;
            eq.set_controls(c); eq.set_self_pitch(255.0f, 1.0f);
            double ls = 0.0, lc = 0.0, hs = 0.0, hc = 0.0;
            for (int i = 0; i < 48000; ++i) {
                const double a = double(TWO_PI) * 255.0 * i / 48000.0;
                const double b = double(TWO_PI) * 4275.0 * i / 48000.0;
                const float x = float(0.25 * (std::sin(a) + std::sin(b)));
                const float y = eq.process_sample(x, x);
                if (i >= 24000) {
                    ls += y * std::sin(a); lc += y * std::cos(a);
                    hs += y * std::sin(b); hc += y * std::cos(b);
                }
            }
            const float low_db = float(20.0 * std::log10(std::hypot(ls, lc) / 3000.0));
            const float high_db = float(20.0 * std::log10(std::hypot(hs, hc) / 3000.0));
            measured[guard] = low_db;
            assert(high_db < -3.0f && high_db > -6.5f);
            if (guard) assert(low_db > -1.2f);
            std::cout << "Q=" << q << " guard=" << guard << " F0=" << low_db
                      << " dB distant=" << high_db << " dB\n";
        }
        assert(measured[1] > measured[0] + 3.0f);
    }
}

int main() {
    test_onion_projection();
    test_cascade_projection();
    test_local_harmonic_protection();
}
