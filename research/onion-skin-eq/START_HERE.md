# Monkey's Ear — active EQ / spectral dynamics research

Status: exploratory research. Synthetic tests exist; real-stem listening validation and product integration are still pending.

## Active direction
Build an excellent conventional EQ first, then extend it into a **Dynamic Spectral Relationship Engine**. Keep acoustic evidence, user intent, and corrective action separate. The active stack is:

1. conventional EQ and clean filter behavior;
2. spectral/temporal evidence and onion-skin history;
3. transient vs sustained/body evidence;
4. directional target/masker and harmonic-family evidence;
5. user intent: Preserve / Yield / Equal;
6. bounded dynamic gain/compression as the primary corrective action;
7. optional phase/spatial/tiny harmonic movement only when evidence and listening justify it;
8. state smoothing/resistance/return to prevent chatter.

## What V1–V6 establish
V1–V3 are historical synthetic overlap heuristics. V4 separates envelope complementarity from overlap. V5 verifies directional gain and silence behavior. V6 adds changing user priority plus bounded harmonic-family gain and movement. V6 reduced its engineering collision proxy from 0.3342 untouched to 0.3293 with dynamic harmonic gain while preserving harmonic-envelope identity above 0.9994. Adding tiny harmonic movement only improved the proxy to 0.3291, so movement remains experimental.

None of these numbers establishes perceptual masking accuracy or audible superiority.

## Decisions
**Keep / strengthen:** excellent conventional EQ; transient/body-aware dynamics; onion-skin history; directional relationships; harmonic-family gain negotiation; bounded user intent; stateful control smoothing; inspectable actions.

**Revise:** the old symmetric masking score becomes raw interaction evidence, not a diagnosis. Dynamic EQ and compression become one shared control-law family rather than separate feature silos. Harmonic relatedness is a grouping descriptor, not an exemption from masking.

**Retire as defaults:** unconstrained harmonic detuning; automatic 'fix the mix' behavior; treating overlap as error; treating the V1–V3 labels as ground truth; tuning thresholds to synthetic labels; magical future lookahead without explicit latency.

## Research priority
Read `V6_HARMONIC_DEPTH.md`, then run `v6_dynamic_harmonic_depth.py`. Next replace the simple collision proxy with a documented auditory-filter/excitation baseline, add transient/body decomposition, and compare against broad EQ and conventional multiband compression. Then move to level-matched real stems and blinded listening labels for clarity, depth, punch, timbral damage and preference.

## Psychoacoustic guardrails
Harmonicity, F0 differences, onset synchrony/common modulation, peripheral frequency resolution and phase can all affect source grouping or masking. Large individual-partial mistuning can create perceptual pop-out; therefore harmonic movement is not a default clarity mechanism. Preserve low/pitch-defining harmonics unless the user explicitly chooses creative transformation.

## Product integration
Inspect current product master and existing EQ/dynamics interfaces before integration. Keep the main agent's MIDI-silence and Chrono-hum fixes as release blockers. Reconcile the research-only GitHub `main` history with the local product branch explicitly; do not force-push over either side. Use existing Lab/Code Prime ownership rather than a parallel rewrite.

For live analysis and control, use bounded/preallocated work, timestamped data and supported VST3 processor/controller communication. No file/network/blocking/unbounded work on the audio callback. Corrective actions must remain opt-in, bounded, automatable, reversible and recallable.

## Primary research record
- `V6_HARMONIC_DEPTH.md` — current design and evidence
- `v6_dynamic_harmonic_depth.py` — current synthetic control-law experiment
- `v6_dynamic_harmonic_depth.csv` — current metrics
- `prototype_v3.py`, V1–V5 CSVs — historical evidence only

## Evidence status
Synthetic engineering tests: active. Real-stem evaluation: pending. Blinded listening: pending. Perceptual masking validation: pending. Product integration: pending.
