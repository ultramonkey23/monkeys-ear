#pragma once

// Offline developmental preset authoring for the production Monkey's Ear engine.
// No allocations, topology mutation, or random generation occur on the audio thread.
// Growth events are deterministic and retained as lineage; the resulting PresetData
// is rendered by the existing MonkeysEarEngine and serializes through existing keys.
#include "monkeys_ear/preset.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace monkeys_ear {

struct GrowthSite {
    float energy = 0.0f;
    float strain = 0.0f;
    float phase = 0.0f;
    float inheritance = 0.0f;
    unsigned branches = 0;
};

struct GrowthNode {
    float x = 0.0f;
    float y = 0.0f;
    float material = 0.0f;
    unsigned parent = 0;
    unsigned site = 0;
};

struct GrowthLineage {
    std::uint32_t seed = 0;
    unsigned events = 0;
    unsigned branches = 0;
    std::array<GrowthSite, 8> sites{};
    // Eight seed vertices followed by persistent branch vertices.
    // Parent indices form an explicit growing graph, not merely branch counters.
    std::array<GrowthNode, 104> nodes{};
    unsigned node_count = 0;
};

struct GrownPreset {
    PresetData preset;
    GrowthLineage lineage;
};

class PresetGrowth {
public:
    // Each site has its own event cadence. A perturbation deposits energy and
    // lasting strain; branching is thresholded on the *local* filtered state.
    static GrownPreset develop(const PresetData& parent, std::uint32_t seed,
                               unsigned steps = 512, float pressure = 0.6f) {
        GrownPreset result{parent, {}};
        result.lineage.seed = seed;
        pressure = std::clamp(pressure, 0.0f, 1.0f);
        std::uint32_t rng = seed ? seed : 0x6d2b79f5u;
        std::array<unsigned, 8> tips{};
        for (unsigned i = 0; i < 8; ++i) {
            const float angle = 6.283185307f * static_cast<float>(i) / 8.0f;
            result.lineage.nodes[i] = {std::cos(angle), std::sin(angle), 0.0f, i, i};
            tips[i] = i;
        }
        result.lineage.node_count = 8;
        std::array<unsigned, 8> periods{};
        std::array<float, 8> lowpass{};
        for (unsigned i = 0; i < 8; ++i) {
            periods[i] = 3u + (next(rng) % 19u);
            result.lineage.sites[i].phase = unit(next(rng)) * 6.283185307f;
        }
        for (unsigned tick = 0; tick < steps; ++tick) {
            for (unsigned i = 0; i < 8; ++i) {
                auto& s = result.lineage.sites[i];
                const auto& neighbor = result.lineage.sites[(i + 7u) % 8u];
                const float coupling = neighbor.energy * 0.09f;
                s.energy = std::clamp(s.energy * 0.976f + coupling, 0.0f, 2.0f);
                s.phase += 0.021f + 0.003f * static_cast<float>(i);
                if ((tick + i) % periods[i] != 0u) continue;
                const float pulse = pressure * (0.20f + unit(next(rng)) * 0.65f);
                lowpass[i] += 0.28f * (pulse - lowpass[i]);
                const float highpass = pulse - lowpass[i];
                s.energy = std::clamp(s.energy + lowpass[i] * 0.28f, 0.0f, 2.0f);
                s.strain = std::clamp(s.strain * 0.98f + std::abs(highpass) * 0.13f,
                                      0.0f, 1.0f);
                ++result.lineage.events;
                if (s.energy > 0.52f && s.branches < 12u &&
                    result.lineage.node_count < result.lineage.nodes.size()) {
                    const unsigned parent_index = tips[i];
                    const auto& parent_node = result.lineage.nodes[parent_index];
                    const float direction = s.phase + 0.32f * (unit(next(rng)) - 0.5f);
                    const float length = 0.11f + 0.08f * s.strain;
                    const unsigned new_index = result.lineage.node_count++;
                    result.lineage.nodes[new_index] = {
                        parent_node.x + length * std::cos(direction),
                        parent_node.y + length * std::sin(direction),
                        s.strain, parent_index, i
                    };
                    tips[i] = new_index;
                    ++s.branches;
                    ++result.lineage.branches;
                    s.inheritance = std::clamp(s.inheritance + 0.075f +
                                              s.strain * 0.05f, 0.0f, 1.0f);
                    s.energy *= 0.53f;
                }
            }
        }
        // The eight persistent sites map to existing *audible* DSP controls.
        // Inheritance survives after event energy decays, so the output is
        // not just a randomized instantaneous modulation snapshot.
        const auto& s = result.lineage.sites;
        const auto mix = [](float a, float b, float depth) {
            return std::clamp(a + b * depth, 0.0f, 1.0f);
        };
        auto& p = result.preset;
        p.wave_a_family = 5; // existing programmable harmonic generator
        p.gen_shape = mix(p.gen_shape, s[0].inheritance, 0.65f);
        p.gen_odd_even = mix(p.gen_odd_even, s[1].inheritance - 0.5f, 0.6f);
        p.gen_tilt = mix(p.gen_tilt, s[2].inheritance - 0.5f, 0.5f);
        p.gen_formant = mix(p.gen_formant, s[3].inheritance, 0.55f);
        p.wave_morph = mix(p.wave_morph, s[4].inheritance, 0.65f);
        p.phase_warp = mix(p.phase_warp, s[5].inheritance - 0.5f, 0.45f);
        p.wave_fold = mix(p.wave_fold, s[6].inheritance, 0.5f);
        p.osc_fm_amount = mix(p.osc_fm_amount, s[7].inheritance, 0.28f);
        p.state_coupling = mix(p.state_coupling, s[1].strain, 0.25f);
        p.state_persistence = mix(p.state_persistence, s[3].inheritance, 0.3f);
        p.performance_drift = mix(p.performance_drift, s[4].strain, 0.35f);
        p.performance_fracture = mix(p.performance_fracture, s[6].strain, 0.3f);
        p.sound_space.enabled = true;
        p.sound_space.modal_mix = mix(p.sound_space.modal_mix, s[7].inheritance, 0.35f);
        p.sound_space.phase_coupling = mix(p.sound_space.phase_coupling, s[2].inheritance, 0.35f);
        char name[64]{};
        std::snprintf(name, sizeof(name), "Growth %08X / %u", seed, result.lineage.branches);
        p.name.assign(name);
        return result;
    }

private:
    static std::uint32_t next(std::uint32_t& state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    static float unit(std::uint32_t bits) {
        return static_cast<float>(bits >> 8) * (1.0f / 16777216.0f);
    }
};

} // namespace monkeys_ear
