# Monkey's Ear — active EQ / spectral dynamics research

Status: exploratory research for the **EQ module only**. This does not define the architecture of the full Monkey's Ear system. Synthetic tests exist; real-stem listening validation and product integration are still pending.

## Product rule
The EQ must be easy to use, but usability and automation are not the research objective. The priority is sound quality and useful control. Automatic behavior is a convenience layer and must not drive the architecture.

Normal EQ use remains familiar: frequency, gain, Q and filter shape. Deeper dynamics, harmonic-family behavior, source relationships, state controls, phase actions and stronger correction are available when the user deliberately configures them.

## Active technical direction
Build an excellent conventional EQ first, then evaluate extensions by whether they improve a defined musical task without unacceptable collateral damage. Keep **evidence, user intent, candidate generation, projection/acceptance, corrective action, and history** separate.

Current priority order:

1. conventional filter quality and phase behavior;
2. dynamic EQ / compression math, including transient vs sustained/body behavior;
3. harmonic-family analysis and candidate gain negotiation for depth/texture preservation;
4. projected bounded action: propose a correction, evaluate it, and reject it when it does not improve the current objective or violates protected structure;
5. directional cross-texture relationships and Preserve / Yield / Equal intent;
6. onion-skin temporal evidence and history;
7. optional state smoothing, persistence, resistance and return where they beat simpler timing laws;
8. advanced phase/spatial/harmonic-motion experiments only when they beat simpler methods for a defined task;
9. automatic behavior last, as a bounded UX layer over proven mechanisms.

## What V1–V9 establish
V1–V3 are historical synthetic overlap heuristics. V4 separates envelope complementarity from overlap. V5 verifies directional gain and silence behavior. V6 adds changing user priority plus bounded harmonic-family gain and movement. V7 stress-tests one subtle automatic controller, but automation is not the core research focus.

V8 compares broad static carve, conventional dynamic EQ, harmonic-family gain control, and relational state dynamics. It showed the tradeoff between raw separation and collateral identity/energy/control motion, and correctly retired "maximize separation" as the objective.

V9 makes the test harder. It introduces an ERB/excitation-neighborhood engineering baseline inspired by auditory-filter literature. A 4,000-case five-strategy sweep found negative lower-decile cases for every fixed-form rule, including V8 harmonic-family control. That means a conflict detector cannot safely authorize a fixed attenuation law by itself.

V9B therefore introduces **projected control**: propose a small bounded action, preserve protected pitch-defining structure, evaluate the action, reject it if it does not improve the current objective, and repeat only while useful bounded improvements remain. Across 2,000 perturbed source pairs, the current projected auditory-harmonic prototype produced zero-or-better ERB-objective change in 100% of cases, held the first four harmonics unchanged in this implementation, kept 1st-percentile full-spectrum identity at 0.998751, and averaged only -0.004861 dB combined energy change.

These remain engineering proxies, not perceptual masking, clarity, depth, or preference scores. Excitation-pattern models themselves are incomplete masking models.

## Current decisions
**PROMOTE:** excellent conventional EQ; conventional dynamic EQ as a required baseline; harmonic-family analysis as a candidate generator; directional user intent; transient/body-aware evidence; inspectable and reversible actions; **projected bounded action as a reusable control primitive**.

**PROMOTE WITH QUALIFICATION:** auditory-weighted harmonic-family targeting. It may propose corrections, but it does not get to commit them without the projection/bounds layer.

**KEEP AS EXPLICIT TOOL:** broad EQ carving. It can be the correct choice when the user wants stronger separation. Its larger timbral change is not automatically a defect.

**REVISE / EXPERIMENTAL:** full relational state dynamics. Persistence, transient protection, user priority, resistance and return remain individually testable ideas, but V8/V9 do not justify a monolithic state engine as the core.

**DEMOTED TO HISTORICAL BASELINE:** V8 fixed harmonic-family attenuation law. Keep it reproducible, but do not implement it directly as product behavior.

**RETIRED AS DEFAULT / OBJECTIVE:** unconstrained harmonic detuning; automatic phase/stereo movement; treating overlap as error; maximizing spectral separation as the sole score; tuning thresholds to synthetic labels; one giant opaque smart-EQ law; magical future lookahead without explicit latency.

## Research priority
Read `V9_AUDITORY_PROJECTED_CONTROL.md` first and run `v9_projected_auditory.py`. `v9_projected_summary.csv` contains the current projected-control summary. Read `V8_CORE_COMPARISON.md` next for the prior four-way baseline and why V9 tightened the architecture.

Next steps are to implement a more faithful documented auditory-filter/excitation reference, make the objective directional target-vs-masker rather than only symmetric overlap, add transient/body and onset-synchrony cases, compare projected conventional dynamic EQ against projected harmonic-family control, then move to real level-matched stems and blinded listening.

Listening labels must cover clarity, depth, punch, timbral damage and preference. Also measure CPU, latency, zippering, worst-case control movement, mono behavior and phase effects.

## Product integration
Inspect current product master and existing EQ/dynamics interfaces before integration. Keep unrelated Monkey's Ear subsystems outside this research scope. Reconcile research-only GitHub `main` history with the product branch explicitly; do not force-push over either side. Use existing Lab/Code Prime ownership rather than a parallel rewrite.

For live analysis and control, use bounded/preallocated work, timestamped data and supported VST3 processor/controller communication. No file/network/blocking/unbounded work on the audio callback. Corrective actions must remain automatable, reversible and recallable.

## Primary research record
- `V9_AUDITORY_PROJECTED_CONTROL.md` — current active decision record
- `v9_projected_auditory.py` — reproducible projected-control simulation
- `v9_projected_summary.csv` — current V9 summary metrics
- `V8_CORE_COMPARISON.md` — previous four-strategy baseline and tradeoff analysis
- `v8_core_comparison.py` / scored CSVs — V8 reproducible evidence
- `V6_HARMONIC_DEPTH.md` — harmonic-depth precursor evidence
- `V7_AUTO_DYNAMICS.md` — UX/automatic-dynamics side research, not current core priority
- `prototype_v3.py`, V1–V5 CSVs — historical evidence only

## Durability rule
Complexity is not progress. A mechanism remains active only if it wins a measured engineering tradeoff, wins a controlled listening task, or provides a clearly different user-controlled creative capability. Tougher evidence can demote a previously promoted idea. Historical results stay reproducible, but active guidance must point to the strongest surviving mechanism rather than preserving old conclusions as doctrine.

## Evidence status
Synthetic engineering tests: active. ERB/excitation engineering baseline: active but deliberately approximate. Real-stem evaluation: pending. Blinded listening: pending. Perceptual masking validation: pending. Product integration: pending.
