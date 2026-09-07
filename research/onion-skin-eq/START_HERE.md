# Monkey's Ear — active EQ / spectral dynamics research

Status: exploratory research for the **EQ module only**. This does not define the architecture of the full Monkey's Ear system. Synthetic tests exist; real-stem listening validation and product integration are still pending.

## UX contract
The EQ must be easy to use. Default dynamics are subtle and automated. Advanced dynamics appear only when the user deliberately opens or configures them.

Normal EQ use should remain familiar: frequency, gain, Q and filter shape, plus at most a simple Dynamic/Auto amount. Threshold, ratio, attack/release, source priority, harmonic-family behavior, state controls, phase actions and stronger gain ranges are advanced/explicit controls, not front-panel requirements.

Automatic corrective behavior must be bounded, transient-aware, low-chatter, reversible and conservative. No automatic harmonic pitch movement, phase manipulation or stereo movement in the default lane.

## Active direction
Build an excellent conventional EQ first, then extend it with optional relational dynamics. Keep acoustic evidence, user intent and corrective action separate. The active stack is:

1. conventional EQ and clean filter behavior;
2. spectral/temporal evidence and onion-skin history;
3. transient vs sustained/body evidence;
4. directional target/masker and harmonic-family evidence;
5. user intent: Preserve / Yield / Equal;
6. subtle bounded dynamic gain/compression as the primary automatic corrective action;
7. stronger dynamics only after explicit user setup;
8. optional phase/spatial/tiny harmonic movement only in advanced/experimental paths when evidence and listening justify it;
9. state smoothing/resistance/return to prevent chatter.

## What V1–V7 establish
V1–V3 are historical synthetic overlap heuristics. V4 separates envelope complementarity from overlap. V5 verifies directional gain and silence behavior. V6 adds changing user priority plus bounded harmonic-family gain and movement. V6 reduced its engineering collision proxy from 0.3342 untouched to 0.3293 with dynamic harmonic gain while preserving harmonic-envelope identity above 0.9994. Adding tiny harmonic movement only improved the proxy to 0.3291, so movement remains experimental.

V7 stress-tests the simple-by-default dynamics contract. With an experimental 1.5 dB automatic ceiling, the controller applied only 0.008 dB during an isolated transient, remained below 1.4 dB peak correction in deterministic stress cases, and showed much less control movement than a deliberately naive 4 dB detector-following baseline. In a separate 30-second randomized evidence stream, automatic reduction stayed below 0.952 dB while an explicitly user-enabled 4 dB mode reached 2.882 dB. These are controller tests, not listening proof.

None of these numbers establishes perceptual masking accuracy or audible superiority.

## Decisions
**Keep / strengthen:** excellent conventional EQ; transient/body-aware dynamics; onion-skin history; directional relationships; harmonic-family gain negotiation; bounded user intent; subtle automatic gain; stateful control smoothing; inspectable actions; explicit advanced dynamics.

**Revise:** the old symmetric masking score becomes raw interaction evidence, not a diagnosis. Dynamic EQ and compression become one shared control-law family rather than separate feature silos. Harmonic relatedness is a grouping descriptor, not an exemption from masking. Automatic dynamics should use persistence and conservative bounds rather than mirror detector level directly.

**Retire as defaults:** unconstrained harmonic detuning; automatic 'fix the mix' behavior; hidden large gain changes; automatic phase/stereo motion; treating overlap as error; treating V1–V3 labels as ground truth; tuning thresholds to synthetic labels; exposing internal state math as mandatory user controls; magical future lookahead without explicit latency.

## Research priority
Read `V7_AUTO_DYNAMICS.md`, then `V6_HARMONIC_DEPTH.md`. Run `v7_auto_dynamics.py` for current automatic-dynamics guardrails and `v6_dynamic_harmonic_depth.py` for harmonic-family research. Next replace the simple collision proxy with a documented auditory-filter/excitation baseline, compare broad EQ / conventional multiband / harmonic-family dynamics / relational state control, and move to level-matched real stems with blinded labels for clarity, depth, punch, timbral damage and preference.

## Product integration
Inspect current product master and existing EQ/dynamics interfaces before integration. Keep unrelated Monkey's Ear subsystems outside this research scope. Reconcile research-only GitHub `main` history with the product branch explicitly; do not force-push over either side. Use existing Lab/Code Prime ownership rather than a parallel rewrite.

For live analysis and control, use bounded/preallocated work, timestamped data and supported VST3 processor/controller communication. No file/network/blocking/unbounded work on the audio callback. Corrective actions must remain automatable, reversible and recallable.

## Primary research record
- `V7_AUTO_DYNAMICS.md` — current usability and dynamics guardrails
- `v7_auto_dynamics.py` — deterministic automatic-dynamics stress simulation
- `v7_auto_dynamics_metrics.csv` — V7 deterministic metrics
- `V6_HARMONIC_DEPTH.md` — harmonic-depth design and evidence
- `v6_dynamic_harmonic_depth.py` — harmonic-depth experiment
- `v6_dynamic_harmonic_depth.csv` — V6 metrics
- `prototype_v3.py`, V1–V5 CSVs — historical evidence only

## Durability rule
A research idea does not stay active because it was once interesting. Promote it only when it improves a defined engineering objective without unacceptable damage or wins a level-matched listening task for a defined musical purpose. Otherwise revise it, move it to an explicitly creative/experimental lane, archive it as historical evidence, or remove it.

## Evidence status
Synthetic engineering tests: active. Real-stem evaluation: pending. Blinded listening: pending. Perceptual masking validation: pending. Product integration: pending.
