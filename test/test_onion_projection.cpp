#ifdef NDEBUG
#undef NDEBUG
#endif
#include "monkeys_ear/onion_projection.h"
#include <cassert>
#include <cmath>
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

int main() { test_onion_projection(); }
