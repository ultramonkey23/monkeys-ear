#pragma once

#include "monkeys_ear/types.h"
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace monkeys_ear {

enum class EvidenceKind : uint8_t { PitchCents, PitchConfidence, NoteGate, Velocity, Onset, Release, Periodicity, Aperiodicity, Loudness, SpectralOccupancy, Motion, Articulation };
enum class EvidenceHorizon : uint8_t { Sample, Micro, Note, Phrase };
enum class InteractionOperator : uint8_t { Attract, Resist, Repel, Dissipate, Inject, Couple };

struct EvidenceProvenance { uint32_t module_instance=0; uint32_t generation=0; };
struct EcosystemEvidence {
    EvidenceKind kind=EvidenceKind::PitchCents; EvidenceHorizon horizon=EvidenceHorizon::Micro;
    EvidenceProvenance provenance{}; float value=0.0f,confidence=0.0f,age_seconds=0.0f,context_low_hz=0.0f,context_high_hz=0.0f;
    bool valid(float max_age_seconds=0.25f) const noexcept {
        return std::isfinite(value)&&std::isfinite(confidence)&&std::isfinite(age_seconds)&&confidence>0.0f&&confidence<=1.0f&&age_seconds>=0.0f&&age_seconds<=max_age_seconds;
    }
};
struct EcosystemInteraction {
    uint32_t source_instance=0; InteractionOperator op=InteractionOperator::Couple; uint32_t destination_instance=0;
    EvidenceKind evidence=EvidenceKind::PitchCents; float amount=0.0f; bool enabled=false;
    bool valid() const noexcept { return enabled&&source_instance!=0&&destination_instance!=0&&std::isfinite(amount)&&amount>=0.0f&&amount<=1.0f; }
};

template <size_t Capacity>
struct EcosystemSnapshot {
    EcosystemEvidence evidence[Capacity]{}; uint16_t count=0; uint32_t generation=0;
    void clear() noexcept { count=0; ++generation; }
    bool push(const EcosystemEvidence& item) noexcept { if(count>=Capacity||!item.valid())return false; evidence[count++]=item; return true; }
    const EcosystemEvidence* freshest(EvidenceKind kind,float max_age_seconds=0.25f) const noexcept {
        const EcosystemEvidence* best=nullptr;
        for(uint16_t i=0;i<count;++i){const auto& item=evidence[i];if(item.kind!=kind||!item.valid(max_age_seconds))continue;
            if(!best||item.age_seconds<best->age_seconds||(item.age_seconds==best->age_seconds&&item.confidence>best->confidence))best=&item;}
        return best;
    }
};

} // namespace monkeys_ear
