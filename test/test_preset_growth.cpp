#include "monkeys_ear/preset_growth.h"
#include "monkeys_ear/engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace monkeys_ear;

struct RenderResult { double energy = 0.0; double signature = 0.0; };

static RenderResult render_energy(const PresetData& preset) {
    MonkeysEarEngine engine;
    engine.init(48000.0f, 256);
    engine.load_preset(preset);
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
    assert(std::abs(recalled.gen_shape - a.preset.gen_shape) < 0.0001f);
    // Exercise the production API, not only the standalone preset compiler.
    MonkeysEarEngine integrated;
    integrated.init(48000.0f, 256);
    integrated.load_preset(parent);
    const auto integrated_lineage = integrated.grow_current_preset(23, 512, 0.65f);
    assert(integrated_lineage.branches == a.lineage.branches);
    assert(integrated.get_current_preset().serialize() == a.preset.serialize());
    const RenderResult e_a = render_energy(a.preset);
    const RenderResult e_b = render_energy(b.preset);
    assert(e_a.energy > 1e-7 && e_b.energy > 1e-7);
    assert(std::abs(e_a.signature - e_b.signature) > 1e-6);
    std::printf("growth preset: branches=%u vs %u, native engine energy=%.8f vs %.8f\n",
                a.lineage.branches, b.lineage.branches, e_a.energy, e_b.energy);
    return 0;
}
