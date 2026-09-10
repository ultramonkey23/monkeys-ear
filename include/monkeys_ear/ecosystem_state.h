#pragma once

#include "monkeys_ear/types.h"
#include <cstdint>
#include <cmath>

namespace monkeys_ear {

// Common language between Monkey's Ear VST3 modules. This is deliberately a
// small value contract, not a router or global registry. Transport/discovery is
// a separate host/integration concern and must never block the audio thread.
enum class EvidenceKind : uint8_t {
    PitchCents,
    PitchConfidence,
    NoteGate,
    Velocity,
    Onset,
    Release,
    Periodicity,
    Aperiodicity,
    Loudness,
    SpectralOccupancy,
    Motion,
    Articulation
};

enum class EvidenceHorizon : uint8_t { Sample, Micro, Note, Phrase };
enum class InteractionOperator : uint8_t { Attract, Resist, Repel, Dissipate, Inject, Couple };

struct EvidenceProvenance {
    uint32_t module_instance = 0;
    uint32_t generation = 0;
};

struct EcosystemEvidence {
    EvidenceKind kind = EvidenceKind::PitchCents;
    EvidenceHorizon horizon = EvidenceHorizon::Micro;
    EvidenceProvenance provenance{};
    float value = 0.0f;
    float confidence = 0.0f;
    float age_seconds = 0.0f;
    float context_low_hz = 0.0f;
    float context_high_hz = 0.0f;

    bool valid(float max_age_seconds = 0.25f) const noexcept {
        return std::isfinite(value) && std::isfinite(confidence) &&
               std::isfinite(age_seconds) && confidence > 0.0f &&
               confidence <= 1.0f && age_seconds >= 0.0f &&
               age_seconds <= max_age_seconds;
    }
};

struct EcosystemInteraction {
    uint32_t source_instance = 0;
    InteractionOperator op = InteractionOperator::Couple;
    uint32_t destination_instance = 0;
    EvidenceKind evidence = EvidenceKind::PitchCents;
    float amount = 0.0f;
    bool enabled = false;

    bool valid() const noexcept {
        return enabled && source_instance != 0 && destination_instance != 0 &&
               std::isfinite(amount) && amount >= 0.0f && amount <= 1.0f;
    }
};

// Fixed-size, allocation-free snapshot that can be copied atomically/by value
// by a future transport. It contains evidence only; it performs no discovery,
// synchronization or modulation itself.
template <size_t Capacity>
struct EcosystemSnapshot {
    EcosystemEvidence evidence[Capacity]{};
    uint16_t count = 0;
    uint32_t generation = 0;

    void clear() noexcept { count = 0; ++generation; }

    bool push(const EcosystemEvidence& item) noexcept {
        if (count >= Capacity || !item.valid()) return false;
        evidence[count++] = item;
        return true;
    }

    const EcosystemEvidence* freshest(EvidenceKind kind, float max_age_seconds = 0.25f) const noexcept {
        const EcosystemEvidence* best = nullptr;
        for (uint16_t i = 0; i < count; ++i) {
            const auto& item = evidence[i];
            if (item.kind != kind || !item.valid(max_age_seconds)) continue;
            if (!best || item.age_seconds < best->age_seconds ||
                (item.age_seconds == best->age_seconds && item.confidence > best->confidence)) best = &item;
        }
        return best;
    }
};

} // namespace monkeys_ear
