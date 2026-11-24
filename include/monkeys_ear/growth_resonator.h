#pragma once
#include "monkeys_ear/preset_growth.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <bit>

namespace monkeys_ear {

// Frozen graph wave body. Growth/compilation is offline; process() performs
// bounded, allocation-free propagation through the actual parent/child edges.
class GrowthResonator {
public:
    static constexpr unsigned kNodes = 104;
    void set_sample_rate(float sample_rate) {
        sample_rate_ = growth_finite(sample_rate) ? std::max(8000.0f, sample_rate) : 48000.0f;
        reset();
    }
    void reset() {
        displacement_.fill(0.0f);
        previous_.fill(0.0f);
        expression_ = 0.0f;
        expression_target_ = 0.0f;
    }
    void disable() { enabled_ = false; node_count_ = 0; reset(); }
    bool enabled() const { return enabled_; }
    // Expressive performance control, not structural mutation. Smoothed per sample.
    void set_expression(float pressure) {
        expression_target_ = growth_finite(pressure) ? std::clamp(pressure,0.0f,1.0f) : 0.0f;
    }

    void compile(const GrowthLineage& graph, float wet = 0.30f) {
        disable();
        if (graph.node_count <= 8 || graph.node_count > kNodes) return;
        node_count_ = graph.node_count;
        wet_ = growth_finite(wet) ? std::clamp(wet, 0.0f, 0.65f) : 0.30f;
        degree_.fill(0);
        pickup_.fill(0.0f);
        for (unsigned i=0; i<node_count_; ++i) {
            const auto& node=graph.nodes[i];
            if (node.site>=8 || !growth_finite(node.material) ||
                node.material<0.0f || node.material>1.0f ||
                !growth_finite(node.x) || !growth_finite(node.y)) { disable(); return; }
            material_[i]=node.material;
            parent_[i]=node.parent;
            if (i>=8) {
                if (node.parent>=i || graph.nodes[node.parent].site!=node.site) {
                    disable(); return;
                }
                ++degree_[i];
                ++degree_[node.parent];
                pickup_[i]=0.18f;
            }
        }
        // The eight persistent roots form the original octagonal ring.
        // Root-to-root links coexist with the inherited branching tree.
        for (unsigned i=0; i<8; ++i) degree_[i]+=2;
        for (unsigned i=0; i<node_count_; ++i)
            pickup_[i] += (i<8 ? 0.75f : 0.0f);
        enabled_=true;
    }

    float process(float input) {
        // Invalid upstream samples must not propagate into the body or output.
        // Preserve the exact dry bypass for all finite legacy inputs.
        if (!growth_finite(input)) input = 0.0f;
        if (!enabled_) return input;
        // Discrete damped graph wave equation. All edges exchange forces using
        // the OLD displacement snapshot, so traversal order cannot bias motion.
        std::array<float,kNodes> force{};
        for (unsigned i=0; i<8; ++i) {
            const unsigned j=(i+1u)&7u;
            const float delta=displacement_[j]-displacement_[i];
            force[i]+=delta;
            force[j]-=delta;
        }
        for (unsigned i=8; i<node_count_; ++i) {
            const unsigned p=parent_[i];
            const float delta=displacement_[p]-displacement_[i];
            force[i]+=delta;
            force[p]-=delta;
        }
        expression_ += (expression_target_-expression_)*0.0015f;
        const float excitation=std::clamp(input,-2.0f,2.0f);
        float pickup=0.0f;
        float pickup_weight=0.0f;
        for (unsigned i=0; i<node_count_; ++i) {
            const float damping=0.035f+0.025f*material_[i]-0.018f*expression_;
            const float coupling=(0.13f+0.09f*expression_)*force[i]/static_cast<float>(std::max(1u,degree_[i]));
            const float injection=(i<8 ? 0.007f : 0.0f)*excitation;
            const float next=(2.0f-damping)*displacement_[i]
                -(1.0f-damping)*previous_[i]+coupling+injection;
            previous_[i]=displacement_[i];
            displacement_[i]=std::clamp(growth_finite(next)?next:0.0f,-2.0f,2.0f);
            pickup+=displacement_[i]*pickup_[i];
            pickup_weight+=pickup_[i];
        }
        const float body=pickup_weight>0.0f ? pickup/pickup_weight : 0.0f;
        return input*(1.0f-wet_)+std::clamp(body*3.0f,-2.0f,2.0f)*wet_;
    }
private:
    std::array<float,kNodes> displacement_{},previous_{},material_{},pickup_{};
    std::array<unsigned,kNodes> parent_{},degree_{};
    unsigned node_count_=0;
    float sample_rate_=48000.0f,wet_=0.30f;
    float expression_=0.0f,expression_target_=0.0f;
    bool enabled_=false;
};

} // namespace monkeys_ear
