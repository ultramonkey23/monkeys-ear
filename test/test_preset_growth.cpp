#include "monkeys_ear/preset_growth.h"
#include "monkeys_ear/engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace monkeys_ear;

struct RenderResult { double energy = 0.0; double signature = 0.0; };

static RenderResult render_energy(const PresetData& preset, bool develop = false) {
    MonkeysEarEngine engine;
    engine.init(48000.0f, 256);
    engine.load_preset(preset);
    if (develop) {
        engine.grow_current_preset(23, 512, 0.65f);
        assert(engine.is_growth_resonator_active());
    }
    engine.handle_midi_note_on(48, 0.85f);
    std::vector<float> left(256), right(256);
    RenderResult result;
    for (unsigned block = 0; block < 80; ++block) {
        engine.process_block(nullptr, nullptr, left.data(), right.data(), left.size());
        for (unsigned i = 0; i < left.size(); ++i) {
            assert(std::isfinite(left[i]) && std::isfinite(right[i]));
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
    assert(a.lineage.events > 0);
    assert(a.lineage.branches > 0);
    assert(a.lineage.node_count == a.lineage.branches + 8);
    assert(a.lineage.node_count <= a.lineage.nodes.size());
    for (unsigned i = 8; i < a.lineage.node_count; ++i) {
        const auto& node = a.lineage.nodes[i];
        assert(node.parent < i);
        assert(node.site < 8);
        assert(std::isfinite(node.x) && std::isfinite(node.y));
    }
    assert(quiet.lineage.branches == 0);
    assert(a.lineage.branches == repeat.lineage.branches);
    assert(a.preset.serialize() == repeat.preset.serialize());
    assert(a.preset.gen_shape != b.preset.gen_shape ||
           a.preset.gen_formant != b.preset.gen_formant ||
           a.preset.wave_fold != b.preset.wave_fold ||
           a.preset.osc_fm_amount != b.preset.osc_fm_amount);
    assert(a.preset.serialize() != quiet.preset.serialize());
    PresetData recalled;
    assert(recalled.deserialize(a.preset.serialize()));
    assert(recalled.wave_a_family == a.preset.wave_a_family);
    GrowthLineage restored_graph{};
    assert(GrowthSnapshotCodec::decode(recalled.growth_snapshot, restored_graph));
    assert(restored_graph.node_count == a.lineage.node_count);
    assert(restored_graph.seed == a.lineage.seed);
    for (unsigned i=0; i<restored_graph.node_count; ++i) {
        assert(restored_graph.nodes[i].parent == a.lineage.nodes[i].parent);
        assert(std::abs(restored_graph.nodes[i].x-a.lineage.nodes[i].x)<0.00001f);
    }
    GrowthLineage rejected{};
    assert(!GrowthSnapshotCodec::decode("G1,23,105", rejected));
    assert(!GrowthSnapshotCodec::decode("G1,23,8,not-a-number", rejected));
    assert(std::abs(recalled.gen_shape - a.preset.gen_shape) < 0.0001f);
    // Exercise the production API, not only the standalone preset compiler.
    MonkeysEarEngine integrated;
    integrated.init(48000.0f, 256);
    integrated.load_preset(parent);
    const auto original_snapshot = integrated.get_current_preset().serialize();
    const auto candidate = integrated.preview_growth_candidate(23, 512, 0.65f);
    assert(integrated.get_current_preset().serialize() == original_snapshot);
    assert(!integrated.is_growth_resonator_active());
    assert(candidate.preset.serialize() == a.preset.serialize());
    const auto integrated_lineage = integrated.grow_current_preset(23, 512, 0.65f);
    assert(integrated_lineage.branches == a.lineage.branches);
    assert(integrated.is_growth_resonator_active());
    assert(integrated.get_current_preset().serialize() == a.preset.serialize());
    // The keeper can be saved, and a descendant explored without mutating it.
    const auto keeper = integrated.get_current_preset();
    const auto child = integrated.preview_growth_candidate(99, 512, 0.65f);
    assert(integrated.get_current_preset().serialize() == keeper.serialize());
    assert(child.preset.serialize() != keeper.serialize());
    const RenderResult e_a = render_energy(a.preset);
    const RenderResult graph_audio = render_energy(parent, true);
    assert(graph_audio.energy > 1e-7);
    // Generated, serialized and recalled frozen candidates must sound the same.
    assert(std::abs(graph_audio.signature - e_a.signature) < 1e-5);
    const RenderResult recalled_audio = render_energy(recalled);
    assert(std::abs(recalled_audio.signature - e_a.signature) < 1e-5);
    PresetData without_graph = a.preset;
    without_graph.growth_snapshot.clear();
    const RenderResult legacy_audio = render_energy(without_graph);
    assert(std::abs(legacy_audio.signature - e_a.signature) > 1e-6);
    MonkeysEarEngine legacy_engine;
    legacy_engine.load_preset(without_graph);
    assert(!legacy_engine.is_growth_resonator_active());
    const RenderResult e_b = render_energy(b.preset);
    assert(e_a.energy > 1e-7 && e_b.energy > 1e-7);
    assert(std::abs(e_a.signature - e_b.signature) > 1e-6);
    std::printf("growth preset: branches=%u vs %u, native engine energy=%.8f vs %.8f\n",
                a.lineage.branches, b.lineage.branches, e_a.energy, e_b.energy);
    return 0;
}
