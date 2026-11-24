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
#include <sstream>
#include <iomanip>
#include <string>

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

// Versioned frozen graph snapshot. Every node is validated before the
// production resonator accepts it. No mutations happen during performance.
struct GrowthSnapshotCodec {
    static std::string encode(const GrowthLineage& lineage) {
        if (lineage.node_count < 8 || lineage.node_count > lineage.nodes.size()) return {};
        std::ostringstream out;
        out << std::setprecision(9) << "G1," << lineage.seed << ',' << lineage.node_count;
        for (unsigned i=0; i<lineage.node_count; ++i) {
            const auto& n=lineage.nodes[i];
            out << ',' << n.x << ',' << n.y << ',' << n.material
                << ',' << n.parent << ',' << n.site;
        }
        return out.str();
    }
    static bool decode(const std::string& snapshot, GrowthLineage& output) {
        if (snapshot.size() > 16384 || snapshot.compare(0,3,"G1,") != 0) return false;
        std::istringstream in(snapshot.substr(3));
        auto comma=[](std::istream& stream) { char c=0; return bool(stream >> c) && c==','; };
        GrowthLineage parsed{};
        if (!(in >> parsed.seed) || !comma(in) || !(in >> parsed.node_count) ||
            parsed.node_count < 8 || parsed.node_count > parsed.nodes.size()) return false;
        for (unsigned i=0; i<parsed.node_count; ++i) {
            auto& n=parsed.nodes[i];
            if (!comma(in) || !(in >> n.x) || !comma(in) || !(in >> n.y) ||
                !comma(in) || !(in >> n.material) || !comma(in) ||
                !(in >> n.parent) || !comma(in) || !(in >> n.site)) return false;
            if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.material) ||
                std::abs(n.x)>1000.0f || std::abs(n.y)>1000.0f ||
                n.material < 0.0f || n.material > 1.0f || n.site >= 8 ||
                (i >= 8 && n.parent >= i)) return false;
        }
        char trailing=0;
        if (in >> trailing) return false;
        parsed.branches=parsed.node_count-8;
        output=parsed;
        return true;
    }
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
        // Descendants inherit the frozen parent's actual graph, rather than
        // restarting from eight vertices and merely inheriting its knob values.
        GrowthLineage ancestor{};
        if (GrowthSnapshotCodec::decode(parent.growth_snapshot, ancestor)) {
            result.lineage.node_count = ancestor.node_count;
            result.lineage.nodes = ancestor.nodes;
            result.lineage.branches = ancestor.branches;
            for (unsigned n=8; n<ancestor.node_count; ++n) {
                const auto& node=ancestor.nodes[n];
                tips[node.site]=n;
                auto& site=result.lineage.sites[node.site];
                ++site.branches;
                site.inheritance=std::clamp(site.inheritance+0.075f+
                                          node.material*0.05f,0.0f,1.0f);
                site.strain=std::max(site.strain,node.material);
            }
        }
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
                if (s.energy > 0.52f &&
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
        // Geometry is a causal input to timbre, not just a visualization.
        // Aggregate graph-edge lengths and directional spread into bounded
        // spectral controls. Identical branch counts can yield different sound.
        float edge_length = 0.0f;
        float directional_spread = 0.0f;
        for (unsigned i = 8; i < result.lineage.node_count; ++i) {
            const auto& n = result.lineage.nodes[i];
            const auto& parent_node = result.lineage.nodes[n.parent];
            const float dx = n.x - parent_node.x;
            const float dy = n.y - parent_node.y;
            edge_length += std::sqrt(dx*dx + dy*dy);
            directional_spread += std::abs(dx - dy);
        }
        const float geometry_length = std::clamp(edge_length / 12.0f, 0.0f, 1.0f);
        const float geometry_spread = std::clamp(directional_spread / 12.0f, 0.0f, 1.0f);
        auto& p = result.preset;
        p.wave_a_family = 5; // existing programmable harmonic generator
        p.gen_shape = mix(p.gen_shape, s[0].inheritance, 0.65f);
        p.gen_odd_even = mix(p.gen_odd_even, s[1].inheritance - 0.5f, 0.6f);
        p.gen_tilt = mix(p.gen_tilt, s[2].inheritance - 0.5f, 0.5f);
        p.gen_tilt = mix(p.gen_tilt, geometry_length - 0.5f, 0.22f);
        p.gen_formant = mix(p.gen_formant, s[3].inheritance, 0.55f);
        p.gen_formant = mix(p.gen_formant, geometry_spread, 0.18f);
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
        p.growth_snapshot = GrowthSnapshotCodec::encode(result.lineage);
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
