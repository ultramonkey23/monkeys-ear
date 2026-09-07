# Monkey's Ear — Onion Skin EQ research

Status: exploratory research, not production DSP or validated perceptual masking.

## Product thesis
Build an excellent conventional EQ first, then add optional temporal, track, and state onion skins. Shared spectral energy is not automatically a problem. Expose interactions and support user-controlled, bounded, recallable corrective relationships. Preserve the existing Monkey's Ear architecture; integrate through the Lab's established Code Prime ownership after inspecting current master.

## Research history
V1: FFT-bin overlap prototype. V2: Bark-like band grouping, relative occupancy, persistence, and transient weighting. V3: collision episodes, longest duration, asymmetry, and advisory severity. V4: separate envelope complementarity from shared energy; define harmonic relatedness and user intent as distinct dimensions.

Synthetic V3 observations: kick/bass Competition (139 ms longest collision); sustained guitars Masking; sustained vocal/guitar Masking; separated sources Coexistence; brief collision Pressure (43 ms). These are heuristic outputs, not ground-truth masking diagnoses. Earlier versions produced false alarms. Do not promote their thresholds to production.

V4 envelope experiment: stable bass and kick overlap 0.09, opposing-motion fraction 0; yielding bass overlap 0.05, opposing-motion fraction 1; following bass overlap 0.09, opposing-motion fraction 0; separated sources overlap 0. These are normalized synthetic descriptors, not perceptual measurements. Complementary motion does not prove absence of masking.

## Next research
Test harmonic relatedness, envelope complementarity, and Preserve/Yield/Equal intent. Harmonic relatedness is a descriptor, not a masking exemption. Preserve protects selected characteristics; Yield permits bounded correction; Equal grants neither source priority. Keep acoustic evidence separate from user intent. Test gain invariance, absolute-level sensitivity, silence, repeated transients, sustained competition, and stereo separation. Use real stems and listening labels to calibrate thresholds. Implement causal streaming history rather than whole-file future knowledge.

## Integration constraints
No changes to the running agent's local checkout. No production integration until current source is inspected. Audio callback must avoid file I/O, network calls, and unbounded analysis. Warnings remain advisory; corrective EQ is opt-in, bounded, automatable, and reversible. Existing MIDI silence and Chrono hum remain release blockers.

## Artifact provenance
The companion conversation artifact is monkeys-ear-onion-skin-research-v4.zip, containing the V3 notebook, V1–V3 result CSVs, V4 envelope CSV, and design notes. The ZIP has not been uploaded to GitHub. This record preserves the research direction and numerical observations; it is not a substitute for the executable artifact or independent validation.
