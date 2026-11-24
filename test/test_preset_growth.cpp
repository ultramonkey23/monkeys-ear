#include "monkeys_ear/preset_growth.h"
#include "monkeys_ear/engine.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

// Always execute verification, including Release/NDEBUG builds.
#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\\n", __FILE__, __LINE__, #expr); \
    std::abort(); \
} } while (false)

using namespace monkeys_ear;

struct RenderResult { double energy = 0.0; double signature = 0.0; };

static RenderResult render_energy(const PresetData& preset, bool develop = false) {
    MonkeysEarEngine engine;
    engine.init(48000.0f, 256);
    engine.load_preset(preset);
    if (develop) {
        engine.grow_current_preset(23, 512, 0.65f);
        CHECK(engine.is_growth_resonator_active());
    }
    engine.handle_midi_note_on(48, 0.85f);
    std::vector<float> left(256), right(256);
    RenderResult result;
    for (unsigned block = 0; block < 80; ++block) {
        engine.process_block(nullptr, nullptr, left.data(), right.data(), left.size());
        for (unsigned i = 0; i < left.size(); ++i) {
            CHECK(std::isfinite(left[i]) && std::isfinite(right[i]));
            result.energy += static_cast<double>(left[i])*left[i] +
                             static_cast<double>(right[i])*right[i];
            result.signature += static_cast<double>(left[i]) *
                                (1.0 + static_cast<double>((block*256+i)%997)/997.0);
        }
    }
    return result;
}

int main() {
    PresetData parent;
    const auto a = PresetGrowth::develop(parent, 23, 512, 0.65f);
    const auto repeat = PresetGrowth::develop(parent, 23, 512, 0.65f);
    const auto b = PresetGrowth::develop(parent, 24, 512, 0.65f);
    const auto quiet = PresetGrowth::develop(parent, 23, 512, 0.0f);
    CHECK(a.lineage.events > 0);
    CHECK(a.lineage.branches > 0);
    CHECK(a.lineage.node_count == a.lineage.branches + 8);
    CHECK(a.lineage.node_count <= a.lineage.nodes.size());
    bool has_fork = false;
    std::array<unsigned,104> child_counts{};
    for (unsigned i=8; i<a.lineage.node_count; ++i) {
        const unsigned parent_index = a.lineage.nodes[i].parent;
        if (++child_counts[parent_index] >= 2u) has_fork = true;
    }
    CHECK(has_fork);
    for (unsigned i = 8; i < a.lineage.node_count; ++i) {
        const auto& node = a.lineage.nodes[i];
        CHECK(node.parent < i);
        CHECK(node.site < 8);
        CHECK(std::isfinite(node.x) && std::isfinite(node.y));
    }
    CHECK(quiet.lineage.branches == 0);
    CHECK(quiet.preset.serialize() == parent.serialize());
    CHECK(PresetGrowth::develop(parent, 23, 0, 0.65f).preset.serialize() == parent.serialize());
    CHECK(PresetGrowth::develop(a.preset, 23, 0, 0.65f).preset.serialize() == a.preset.serialize());
    CHECK(PresetGrowth::develop(a.preset, 23, 512, 0.0f).preset.serialize() == a.preset.serialize());
    CHECK(a.lineage.branches == repeat.lineage.branches);
    CHECK(a.preset.serialize() == repeat.preset.serialize());
    // Two seeds can legitimately converge on the same bounded knob values.
    // Distinguish structural descendants by actual graph coordinates instead.
    bool geometry_differs = a.lineage.node_count != b.lineage.node_count;
    for (unsigned i=8; i<std::min(a.lineage.node_count,b.lineage.node_count); ++i) {
        geometry_differs |= (a.lineage.nodes[i].x != b.lineage.nodes[i].x ||
                             a.lineage.nodes[i].y != b.lineage.nodes[i].y);
    }
    CHECK(geometry_differs);
    CHECK(a.preset.serialize() != quiet.preset.serialize());
    PresetData recalled;
    CHECK(recalled.deserialize(a.preset.serialize()));
    CHECK(recalled.wave_a_family == a.preset.wave_a_family);
    // Reusing a preset object to read an old patch must discard prior topology.
    PresetData legacy_source;
    CHECK(recalled.deserialize(legacy_source.serialize()));
    CHECK(recalled.growth_snapshot.empty());
    CHECK(recalled.deserialize(a.preset.serialize()));
    GrowthLineage restored_graph{};
    CHECK(GrowthSnapshotCodec::decode(recalled.growth_snapshot, restored_graph));
    CHECK(restored_graph.node_count == a.lineage.node_count);
    CHECK(restored_graph.seed == a.lineage.seed);
    for (unsigned i=0; i<restored_graph.node_count; ++i) {
        CHECK(restored_graph.nodes[i].parent == a.lineage.nodes[i].parent);
        CHECK(std::abs(restored_graph.nodes[i].x-a.lineage.nodes[i].x)<0.00001f);
    }
    GrowthLineage rejected{};
    CHECK(!GrowthSnapshotCodec::decode("G1,23,105", rejected));
    CHECK(!GrowthSnapshotCodec::decode("G1,23,8,not-a-number", rejected));
    CHECK(std::abs(recalled.gen_shape - a.preset.gen_shape) < 0.0001f);
    // Exercise the production API, not only the standalone preset compiler.
    MonkeysEarEngine integrated;
    integrated.init(48000.0f, 256);
    integrated.load_preset(parent);
    const auto original_snapshot = integrated.get_current_preset().serialize();
    const auto candidate = integrated.preview_growth_candidate(23, 512, 0.65f);
    CHECK(integrated.get_current_preset().serialize() == original_snapshot);
    CHECK(!integrated.is_growth_resonator_active());
    CHECK(candidate.preset.serialize() == a.preset.serialize());
    const auto integrated_lineage = integrated.grow_current_preset(23, 512, 0.65f);
    CHECK(integrated_lineage.branches == a.lineage.branches);
    CHECK(integrated.is_growth_resonator_active());
    CHECK(integrated.get_current_preset().serialize() == a.preset.serialize());
    // The keeper can be saved, and a descendant explored without mutating it.
    const auto keeper = integrated.get_current_preset();
    const auto child = integrated.preview_growth_candidate(99, 512, 0.65f);
    CHECK(integrated.get_current_preset().serialize() == keeper.serialize());
    CHECK(child.preset.serialize() != keeper.serialize());
    CHECK(child.lineage.node_count >= integrated_lineage.node_count);
    for (unsigned i=0; i<integrated_lineage.node_count; ++i) {
        CHECK(child.lineage.nodes[i].parent == integrated_lineage.nodes[i].parent);
        CHECK(child.lineage.nodes[i].x == integrated_lineage.nodes[i].x);
        CHECK(child.lineage.nodes[i].y == integrated_lineage.nodes[i].y);
    }
    // Sample-rate changes must retain the frozen graph and rebuild DSP.
    integrated.init(44100.0f, 128);
    CHECK(integrated.is_growth_resonator_active());
    CHECK(integrated.get_current_preset().serialize() == keeper.serialize());
    const RenderResult e_a = render_energy(a.preset);
    const RenderResult graph_audio = render_energy(parent, true);
    CHECK(graph_audio.energy > 1e-7);
    // Generated, serialized and recalled frozen candidates must sound the same.
    CHECK(std::abs(graph_audio.signature - e_a.signature) < 1e-5);
    const RenderResult recalled_audio = render_energy(recalled);
    CHECK(std::abs(recalled_audio.signature - e_a.signature) < 1e-5);
    PresetData without_graph = a.preset;
    without_graph.growth_snapshot.clear();
    const RenderResult legacy_audio = render_energy(without_graph);
    CHECK(std::abs(legacy_audio.signature - e_a.signature) > 1e-6);
    MonkeysEarEngine legacy_engine;
    legacy_engine.load_preset(without_graph);
    CHECK(!legacy_engine.is_growth_resonator_active());
    const RenderResult e_b = render_energy(b.preset);
    CHECK(e_a.energy > 1e-7 && e_b.energy > 1e-7);
    CHECK(std::abs(e_a.signature - e_b.signature) > 1e-6);
    std::printf("growth preset: branches=%u vs %u, native engine energy=%.8f vs %.8f\n",
                a.lineage.branches, b.lineage.branches, e_a.energy, e_b.energy);
    return 0;
}
