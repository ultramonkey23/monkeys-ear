#pragma once

#include "monkeys_ear/types.h"
#include <array>
#include <cstdint>

namespace monkeys_ear {

// Every shippable Monkey's Ear module must satisfy this contract independently.
// A module may share DSP primitives, but must not require another module to be
// instantiated in order to produce valid audio/state in a DAW such as REAPER.
enum class ModuleKind : uint8_t {
    Instrument,
    AudioEffect,
    MidiEffect
};

enum class ControlDepth : uint8_t {
    Play,      // immediate musical controls; useful without documentation
    Advanced,  // meaningful mechanism controls
    Lab        // deep/experimental controls with bounded behavior
};

struct ModuleIdentity {
    const char* id = "";           // stable machine id; never localized
    const char* name = "";         // host-facing product name
    const char* category = "";     // concise host/browser category
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

// Small common lifecycle surface for standalone wrappers. No allocation,
// strings, locks or host calls are required by the audio-thread methods.
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

// Host/state values are untrusted. Keep one canonical normalization rule for
// standalone wrappers and the eventual combined suite.
inline float sanitize_normalized_parameter(float value, float fallback = 0.0f) noexcept {
    value = sanitize(value);
    fallback = clamp(sanitize(fallback), 0.0f, 1.0f);
    return value >= 0.0f && value <= 1.0f ? value : fallback;
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

    // Requires an actual REAPER/user pass; code must never manufacture this.
    bool passes_product_gate() const noexcept {
        return passes_engineering_gate() && project_recall && primary_controls_obvious;
    }
};

} // namespace monkeys_ear
