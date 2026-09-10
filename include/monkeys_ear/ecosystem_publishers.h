#pragma once

#include "monkeys_ear/ecosystem_state.h"
#include <cmath>

namespace monkeys_ear {

inline float hz_to_ecosystem_cents(float hz) noexcept {
    if (!std::isfinite(hz) || hz <= 0.0f) return 0.0f;
    return 1200.0f * std::log2(hz / 440.0f);
}

// Adapter around existing analysis. It does not own or duplicate the detector.
inline void publish_tonal_evidence(EcosystemSnapshot<32>& out, uint32_t module_instance,
                                   float tracked_hz, float confidence) noexcept {
    if (!std::isfinite(tracked_hz) || tracked_hz < 20.0f || tracked_hz > 5000.0f) return;
    confidence = clamp(sanitize(confidence), 0.0f, 1.0f);
    if (confidence <= 0.0f) return;
    EcosystemEvidence pitch{}; pitch.kind=EvidenceKind::PitchCents; pitch.horizon=EvidenceHorizon::Micro;
    pitch.provenance={module_instance,out.generation}; pitch.value=hz_to_ecosystem_cents(tracked_hz); pitch.confidence=confidence; out.push(pitch);
    EcosystemEvidence certainty=pitch; certainty.kind=EvidenceKind::PitchConfidence; certainty.value=confidence; out.push(certainty);
}

inline void publish_midi_note_evidence(EcosystemSnapshot<32>& out, uint32_t module_instance,
                                       int midi_note, float velocity, bool gate) noexcept {
    midi_note = clamp(midi_note, 0, 127); velocity = clamp(sanitize(velocity),0.0f,1.0f);
    EcosystemEvidence pitch{}; pitch.kind=EvidenceKind::PitchCents; pitch.horizon=EvidenceHorizon::Note;
    pitch.provenance={module_instance,out.generation}; pitch.value=(static_cast<float>(midi_note)-69.0f)*100.0f;
    pitch.confidence=gate?1.0f:0.5f; out.push(pitch);
    EcosystemEvidence gate_ev=pitch; gate_ev.kind=EvidenceKind::NoteGate; gate_ev.value=gate?1.0f:0.0f; gate_ev.confidence=1.0f; out.push(gate_ev);
    EcosystemEvidence vel=pitch; vel.kind=EvidenceKind::Velocity; vel.value=velocity; vel.confidence=1.0f; out.push(vel);
}

} // namespace monkeys_ear
