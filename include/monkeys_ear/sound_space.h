#pragma once

#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

enum class SpaceSource { LFO=0, Envelope, FastEnergy, SlowEnergy, Direction, Aftertouch, Velocity, AudioEnergy };
enum class SpaceDestination { PitchBound=0, HarmonicBound, ModalBound, MovementRate, Attraction, SpectralDepth, Phase, HarmonicMix };

struct SpaceRoute { int source=0; int destination=0; float depth=0.0f; };

struct SoundSpaceControls {
    bool enabled=false;
    float pitch_bound_cents=0.0f, harmonic_bound_cents=0.0f, modal_bound_cents=0.0f;
    float movement_hz=0.5f, attraction=0.5f, resistance=0.5f, repulsion=0.5f;
    float frequency_freedom=0.5f, energy_widen=0.0f, attack_freedom=0.0f, release_relaxation=0.5f;
    float pitch_mix=0.0f, harmonic_mix=0.0f, modal_mix=0.0f;
    float spectral_depth_db=0.0f, spectral_priority=0.5f;
    float phase_offset_cycles=0.0f, phase_coupling=0.5f;
    std::array<SpaceRoute,4> routes{};
};

struct SoundSpaceSources {
    float lfo=0.0f, envelope=0.0f, fast_energy=0.0f, slow_energy=0.0f;
    float direction=0.0f, aftertouch=0.0f, velocity=0.0f, audio_energy=0.0f;
};

struct SoundSpaceMetrics {
    float pitch_cents=0.0f, max_harmonic_cents=0.0f, max_modal_cents=0.0f;
    float spectral_gain_db=0.0f, weight_fast=0.0f, weight_slow=0.0f;
};

class BoundedMicrofield {
public:
    void reset(float seed=0.0f);
    float process(float bound_cents, float movement_hz, float attraction,
                  float resistance, float repulsion, float drive, float dt);
    float normalized_position() const { return position_; }
private:
    float position_=0.0f, velocity_=0.0f, phase_=0.0f;
};

class SoundSpaceProcessor {
public:
    static constexpr size_t HARMONICS=6, MODES=4;
    void set_sample_rate(float sr);
    void set_controls(const SoundSpaceControls& controls) { controls_=controls; }
    void reset();
    float process(float character, float weight, float center_hz, const SoundSpaceSources& sources);
    const SoundSpaceMetrics& metrics() const { return metrics_; }
private:
    struct Resonator { float y1=0.0f,y2=0.0f; };
    float sample_rate_=48000.0f, pitch_phase_=0.0f, low_state_=0.0f, allpass_z_=0.0f;
    float weight_fast_=0.0f, weight_slow_=0.0f, spectral_gain_db_=0.0f;
    SoundSpaceControls controls_{};
    SoundSpaceMetrics metrics_{};
    BoundedMicrofield pitch_field_{};
    std::array<BoundedMicrofield,HARMONICS> harmonic_fields_{};
    std::array<BoundedMicrofield,MODES> modal_fields_{};
    std::array<float,HARMONICS> harmonic_phase_{};
    std::array<Resonator,MODES> resonators_{};
    float source_value(int source, const SoundSpaceSources& s) const;
};

} // namespace monkeys_ear
