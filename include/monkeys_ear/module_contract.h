#pragma once

#include "monkeys_ear/types.h"
#include <cstdint>
#include <cmath>

namespace monkeys_ear {

enum class ModuleKind : uint8_t { Instrument, AudioEffect, MidiEffect };
enum class ControlDepth : uint8_t { Play, Advanced, Lab };

struct ModuleIdentity {
    const char* id = "";
    const char* name = "";
    const char* category = "";
    ModuleKind kind = ModuleKind::AudioEffect;
    uint32_t state_version = 1;
};

struct ModuleIOContract {
    uint8_t audio_inputs = 0;
    uint8_t audio_outputs = 2;
    bool accepts_midi = false;
    bool produces_midi = false;
    bool reports_zero_latency = true;
};

struct ModuleHealth {
    bool finite_output = true;
    bool state_valid = true;
    bool processing_ready = false;
};

class StandaloneModule {
public:
    virtual ~StandaloneModule() = default;
    virtual const ModuleIdentity& identity() const noexcept = 0;
    virtual ModuleIOContract io_contract() const noexcept = 0;
    virtual void prepare(float sample_rate, uint32_t max_block_size) noexcept = 0;
    virtual void reset() noexcept = 0;
    virtual void set_bypass(bool bypass) noexcept = 0;
    virtual bool bypassed() const noexcept = 0;
    virtual ModuleHealth health() const noexcept = 0;
};

// Host/state values are untrusted. Non-finite values use the fallback; ordinary
// finite automation overshoot clamps to the nearest legal normalized endpoint.
inline float sanitize_normalized_parameter(float value, float fallback = 0.0f) noexcept {
    fallback = std::isfinite(fallback) ? clamp(fallback, 0.0f, 1.0f) : 0.0f;
    if (!std::isfinite(value)) return fallback;
    return clamp(value, 0.0f, 1.0f);
}

struct ReaperStandaloneGate {
    bool instantiates = false;
    bool correct_io = false;
    bool finite_silence = false;
    bool finite_signal = false;
    bool bypass_transparent = false;
    bool automation_safe = false;
    bool state_roundtrip = false;
    bool project_recall = false;
    bool sample_rate_change = false;
    bool buffer_size_change = false;
    bool offline_render = false;
    bool primary_controls_obvious = false;

    bool passes_engineering_gate() const noexcept {
        return instantiates && correct_io && finite_silence && finite_signal &&
               bypass_transparent && automation_safe && state_roundtrip &&
               sample_rate_change && buffer_size_change && offline_render;
    }
    bool passes_product_gate() const noexcept {
        return passes_engineering_gate() && project_recall && primary_controls_obvious;
    }
};

} // namespace monkeys_ear
