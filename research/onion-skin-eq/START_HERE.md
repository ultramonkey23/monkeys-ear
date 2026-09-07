# Monkey's Ear — active EQ / spectral dynamics research

Status: exploratory research for the **EQ module only**. This does not define the architecture of the full Monkey's Ear system. Synthetic tests exist; real-stem listening validation and product integration are still pending.

## Product rule
The EQ must be easy to use, but usability is not the research objective. The priority is sound quality and useful control. Automatic behavior is a convenience layer and must not drive the architecture.

Normal EQ use remains familiar: frequency, gain, Q and filter shape. Deeper dynamics, harmonic-family behavior, source relationships, state controls, phase actions and stronger correction are available when the user deliberately configures them.

## Active technical direction
Build an excellent conventional EQ first, then evaluate extensions by whether they improve a defined musical task without unacceptable collateral damage. Keep evidence, user intent and corrective action separate.

Current priority order:

1. conventional filter quality and phase behavior;
2. dynamic EQ / compression math, including transient vs sustained/body behavior;
3. harmonic-family gain negotiation for depth and texture preservation;
4. directional cross-texture relationships and Preserve / Yield / Equal intent;
5. onion-skin temporal evidence and history;
6. optional state smoothing, persistence, resistance and return;
7. advanced phase/spatial/harmonic-motion experiments only when they beat simpler methods for a defined task;
8. automatic behavior last, as a bounded UX layer over proven mechanisms.

## What V1–V8 establish
V1–V3 are historical synthetic overlap heuristics. V4 separates envelope complementarity from overlap. V5 verifies directional gain and silence behavior. V6 adds changing user priority plus bounded harmonic-family gain and movement. V7 stress-tests one subtle automatic controller, but automation is no longer the research focus.

V8 directly compares four core strategies: broad static carve, conventional dynamic EQ, harmonic-family gain control, and relational state dynamics. In the deterministic matched case, broad carving produced the largest reduction in the simple collision proxy, but also caused more identity/energy change and more control motion. Harmonic-family control produced a smaller separation change while preserving source identity at 0.9998+ with much less motion. A 2,000-state randomized sweep reproduced the same tradeoff.

These are engineering proxies, not perceptual masking, clarity, depth, or preference scores.

## Current decisions
**PROMOTE:** excellent conventional EQ; conventional dynamic EQ as a required baseline; harmonic-family gain negotiation as the strongest distinctive mechanism currently supported by the synthetic evidence; directional user intent; transient/body-aware control; inspectable and reversible actions.

**KEEP AS EXPLICIT TOOL:** broad EQ carving. It can be the correct choice when the user wants stronger separation. Its larger timbral change is not automatically a defect.

**REVISE / EXPERIMENTAL:** relational state dynamics. Persistence, transient protection, user priority, resistance and return remain promising, but V8 does not justify making the entire state system the core. Split useful mechanisms out and retest independently.

**RETIRED AS DEFAULT / OBJECTIVE:** unconstrained harmonic detuning; automatic phase/stereo movement; treating overlap as error; maximizing spectral separation as the sole score; tuning thresholds to synthetic labels; one giant opaque 'smart EQ' law; magical future lookahead without explicit latency.

## Research priority
Read `V8_CORE_COMPARISON.md` first. Run `v8_core_comparison.py` to reproduce the current four-way core comparison. `v8_core_comparison_scored.csv` contains the deterministic summary and `v8_monte_carlo_summary.csv` contains the 2,000-state randomized sweep summary.

Next, replace the simple collision proxy with a documented auditory-filter / excitation-pattern baseline, then run the same four-way comparison on level-matched real stems. Listening labels must cover clarity, depth, punch, timbral damage and preference. Also measure CPU, latency, zippering, worst-case control movement, mono behavior and phase effects.

## Product integration
Inspect current product master and existing EQ/dynamics interfaces before integration. Keep unrelated Monkey's Ear subsystems outside this research scope. Reconcile research-only GitHub `main` history with the product branch explicitly; do not force-push over either side. Use existing Lab/Code Prime ownership rather than a parallel rewrite.

For live analysis and control, use bounded/preallocated work, timestamped data and supported VST3 processor/controller communication. No file/network/blocking/unbounded work on the audio callback. Corrective actions must remain automatable, reversible and recallable.

## Primary research record
- `V8_CORE_COMPARISON.md` — current core strategy decisions
- `v8_core_comparison.py` — reproducible deterministic comparison
- `v8_core_comparison_scored.csv` — deterministic metrics
- `v8_monte_carlo_summary.csv` — randomized sweep summary
- `V6_HARMONIC_DEPTH.md` — harmonic-depth precursor evidence
- `V7_AUTO_DYNAMICS.md` — UX/automatic-dynamics side research, not current core priority
- `prototype_v3.py`, V1–V5 CSVs — historical evidence only

## Durability rule
Complexity is not progress. A mechanism remains active only if it wins a measured engineering tradeoff, wins a controlled listening task, or provides a clearly different user-controlled creative capability. Otherwise simplify it, revise it, move it to an explicit experimental lane, archive it as historical evidence, or remove it.

## Evidence status
Synthetic engineering tests: active. Real-stem evaluation: pending. Blinded listening: pending. Perceptual masking validation: pending. Product integration: pending.
