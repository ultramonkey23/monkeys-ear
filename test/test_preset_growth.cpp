#include "monkeys_ear/preset_growth.h"
#include "monkeys_ear/engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace monkeys_ear;

struct RenderResult { double energy = 0.0; double signature = 0.0; };\n\nstatic RenderResult render_energy(const PresetData& preset) {
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
            energy += static_cast<double>(left[i])*left[i] +
                      static_cast<double>(right[i])*right[i];
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
    assert(a.preset.gen_shape != b.preset.gen_shape ||\n           a.preset.gen_formant != b.preset.gen_formant ||\n           a.preset.wave_fold != b.preset.wave_fold ||\n           a.preset.osc_fm_amount != b.preset.osc_fm_amount);
    assert(a.preset.serialize() != quiet.preset.serialize());
    PresetData recalled;
    assert(recalled.deserialize(a.preset.serialize()));
    assert(recalled.wave_a_family == a.preset.wave_a_family);
    assert(std::abs(recalled.gen_shape - a.preset.gen_shape) < 0.0001f);
    const double e_a = render_energy(a.preset);
    const double e_b = render_energy(b.preset);
    assert(e_a.energy > 1e-7 && e_b.energy > 1e-7);\n    assert(std::abs(e_a.signature - e_b.signature) > 1e-6);
    std::printf("growth preset: branches=%u vs %u, native engine energy=%.8f vs %.8f\n",
                a.lineage.branches, b.lineage.branches, e_a.energy, e_b.energy);
    return 0;
}
