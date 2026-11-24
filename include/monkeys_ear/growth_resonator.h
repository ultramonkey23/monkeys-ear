#pragma once
#include "monkeys_ear/preset_growth.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

// Fixed-cost real-time resonant network compiled from a developed growth graph.
// Compilation is control-thread only. Per-sample processing allocates nothing.
class GrowthResonator {
public:
    static constexpr unsigned kModes = 8;
    void set_sample_rate(float sample_rate) {
        sample_rate_ = std::max(8000.0f, sample_rate);
        reset();
    }
    void reset() {
        for (auto& mode : modes_) { mode.y1 = 0.0f; mode.y2 = 0.0f; }
    }
    void disable() { enabled_ = false; reset(); }
    bool enabled() const { return enabled_; }

    void compile(const GrowthLineage& graph, float wet = 0.30f) {
        reset();
        wet_ = std::clamp(wet, 0.0f, 0.65f);
        enabled_ = graph.node_count > 8 && graph.node_count <= graph.nodes.size();
        if (!enabled_) return;
        std::array<float,kModes> length{};
        std::array<float,kModes> strain{};
        std::array<unsigned,kModes> counts{};
        for (unsigned n=8; n<graph.node_count; ++n) {
            const auto& node=graph.nodes[n];
            if (node.site >= kModes || node.parent >= n) continue;
            const auto& parent=graph.nodes[node.parent];
            const float dx=node.x-parent.x, dy=node.y-parent.y;
            length[node.site] += std::sqrt(dx*dx+dy*dy);
            strain[node.site] += node.material;
            ++counts[node.site];
        }
        for (unsigned i=0; i<kModes; ++i) {
            auto& m=modes_[i];
            const float mean_strain=counts[i] ? strain[i]/counts[i] : 0.0f;
            // Length, branching and inherited material all influence resonance.
            const float hz=std::clamp(105.0f + 68.0f*static_cast<float>(i)
                           + 195.0f/(1.0f+length[i])
                           + 95.0f*mean_strain, 45.0f, sample_rate_*0.40f);
            const float radius=std::clamp(0.9970f - 0.00014f*static_cast<float>(counts[i])
                                        -0.0010f*mean_strain, 0.985f, 0.998f);
            const float omega=6.283185307179586f*hz/sample_rate_;
            m.a=2.0f*radius*std::cos(omega);
            m.b=radius*radius;
            m.gain=counts[i] ? 0.008f/(1.0f+0.14f*counts[i]) : 0.0f;
        }
    }

    float process(float input) {
        if (!enabled_) return input;
        float resonance=0.0f;
        for (auto& m:modes_) {
            // Bounded resonator: stable poles and saturated excitation.
            const float excitation=std::clamp(input,-2.0f,2.0f)*m.gain;
            const float y=excitation + m.a*m.y1 - m.b*m.y2;
            m.y2=m.y1;
            m.y1=std::clamp(y,-4.0f,4.0f);
            resonance+=m.y1;
        }
        return input*(1.0f-wet_)+std::clamp(resonance,-2.0f,2.0f)*wet_;
    }
private:
    struct Mode { float y1=0.0f,y2=0.0f,a=0.0f,b=0.0f,gain=0.0f; };
    std::array<Mode,kModes> modes_{};
    float sample_rate_=48000.0f,wet_=0.30f;
    bool enabled_=false;
};

} // namespace monkeys_ear
