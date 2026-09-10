# Monkey's Ear Vocal — Perceptual, Usability, and Robustness Research

Date: 2026-09-10
Status: design/validation program; not listening proof
Scope: refine the frozen vocal feature set. Do not add feature families.

## Why this pass exists

Monkey's Ear has accumulated strong DSP ideas, but algorithmic sophistication alone does not make an instrument sound good, feel easy, or survive hostile host/input conditions. This pass adds three missing design axes:

1. perceptual quality — where controls actually sound useful;
2. usability — how deep mechanisms remain discoverable and predictable;
3. robustness — how every mechanism behaves under bad input, automation, state, and host lifecycle events.

The goal is not to remove deep controls. Monkey's Ear should expose deep and even difficult controls for users who want them. The goal is to make the default path immediately musical while the deep path remains coherent rather than arbitrary.

## Research findings

### Perceptual ranges matter more than linear numeric ranges

DAFx listening research on modulation effects found perceptual regions corresponding roughly to too-subtle, musically useful, and too-extreme behavior. The direct effect studied was not pitch correction, so the exact ranges do not transfer. The useful design principle does: control ranges should be tuned from perception, not from convenient mathematics.

For Monkey's Ear, every important continuous control should eventually have an empirically found map:

- neutral/dead zone when useful;
- subtle region;
- broad musical region;
- extreme/character region;
- unsafe/undefined region, which is never exposed.

Do not truncate the extreme region merely because it is unusual. Character is part of the product. Instead, give the normal surface more resolution in the musically productive region while DEEP mode can expose the entire valid domain.

### Control scaling is part of the effect

Adaptive-DAFx literature explicitly discusses control-curve scaling/zooming because useful sonic behavior can occupy only part of a raw parameter range. Therefore UI normalization and DSP parameterization should be separate.

Recommended contract:

```
normalized UI value
    -> perceptual mapping
    -> bounded semantic parameter
    -> topology-safe smoothing/transition
    -> DSP
```

A 0..1 UI control does not imply a linear DSP mapping.

### Adaptive effects should map sound evidence to bounded control changes

Adaptive-DAFx work frames effects as feature extraction -> mapping -> effect parameter. This aligns with Monkey's Ear's shared vocal-analysis direction. Analysis should advise the effect inside explicit bounds; it should not freely rewrite topology or produce unbounded parameters.

Example: onset evidence may move compressor attack within a defined musical interval. It should not directly become an attack time with an arbitrary formula.

### Real-time robustness is a product feature

Open-source real-time DSP guidance repeatedly converges on the same failure classes: allocations, locks, I/O, denormals, invalid host parameters, dirty reset state, unsafe buffer assumptions, and unsmoothed controls. Monkey's Ear already values hard RT behavior; this pass turns that into explicit invariants and abuse tests.

## Corrected scale-system guidance

The previous open-source mining note overcomplicated the scale system by recommending that gravity must be split into many mandatory musical semantics. That is not the desired product direction.

Keep the scale system conceptually simple at its core:

- degrees/ratios/cents;
- enabled state;
- gravity;
- directional behavior;
- arbitrary period/non-octave support;
- trajectory/history/articulation handled outside the scale definition.

Deep controls MAY expose additional real mechanisms such as capture width or release behavior if they produce useful audible behavior, but they are optional expert controls, not required theory or mandatory degree roles.

Principle:

> Scale defines where pitch is attracted. Performance analysis and trajectory logic determine how the voice gets there.

Do not force users or presets to classify every degree through a large theory ontology.

## Three-layer control model

### PLAY

Immediate musical surface. Few controls, large useful ranges, strong defaults.

Candidate vocabulary remains:

`TUNE · PITCH · FORMANT · ARTICULATION · VIBRATO · DYNAMICS · DRIVE · CHARACTER`

### ADVANCED

Direct musician-facing mechanisms such as correction strength, drift retention, vibrato amount, transition, gravity, formant follow, compressor timing, saturation character.

### DEEP / LAB

Expose genuine mechanism controls even when they require expertise: capture width, hysteresis, transform authority, PSOLA stage distribution, source-component weights, envelope parameters, antialias mode, detector weighting, etc.

Hard-to-understand knobs are allowed here. They must still have a deterministic effect, stable bounds, recall/automation semantics, and a short tooltip or documentation statement describing what mechanism changes.

Do not add a knob solely because an internal variable exists.

## Parameter interaction contract

The major usability risk is not the number of controls; it is controls silently fighting one another.

Establish ownership:

- Tuning space owns candidate destinations.
- Target/trajectory owns target continuity and movement.
- Correction owns movement toward target.
- Intentional pitch shift owns transposition after musical correction intent is established.
- Vibrato/drift controls own their decomposed expression components.
- Formant owns spectral-envelope/identity transformation, not F0 target selection.
- Dynamics owns gain trajectory.
- Saturation owns nonlinear character.
- Output safety always wins over creative intent.

Cross-feature macros may coordinate several owners, but the macro should produce bounded semantic targets rather than bypassing ownership.

## Parameter-transition taxonomy

Do not apply one generic lerp to every control.

### Sample-smoothed
Use for continuous scalar values whose interpolation is itself meaningful: gain, mix, drive amount, continuous pitch offset where appropriate.

### Trajectory-smoothed
Use for musical quantities where motion shape matters: correction amount, target trajectory, formant shift, vibrato depth, drift amount.

### Crossfaded
Use when two valid DSP states/topologies cannot safely be coefficient-interpolated or when changing algorithm families would click.

### State-switched at safe boundaries
Use for structural choices where a short controlled handoff is better than continuous interpolation.

### Prepared off audio thread
FFT plans, file data, large table rebuilds, graph changes, or anything allocating/blocking must be prepared away from the callback and handed off safely.

## Failure-degradation doctrine

The vocal engine must have a boring safe answer for uncertainty.

Primary invariant:

> As analysis confidence/source suitability falls, aggressive transformation authority should generally fall toward dry/neutral behavior rather than become more chaotic accidentally.

This does not prevent intentional chaos. Creative instability is an explicit bounded control, not a side effect of detector failure.

Examples:

- invalid/zero/NaN F0 -> no harmonic pitch transform;
- low confidence -> retain source and target history conservatively;
- aperiodic/transient material -> protect rather than force through PSOLA;
- empty/invalid tuning -> deterministic safe fallback, never undefined indexing;
- corrupt preset values -> sanitize/clamp/default;
- non-finite intermediate/output -> deterministic finite fallback and safety stage;
- reset/sample-rate change -> all history explicitly reinitialized;
- silence -> no runaway history, denormal CPU spike, or stale target surprise.

## Robustness matrix

Every vocal subsystem should eventually be exercised against:

- silence and near-silence;
- impulses;
- DC;
- clipped/hot input;
- NaN/Inf at public boundaries where host/input can supply them;
- rapidly alternating pitch confidence;
- octave-error F0 sequences;
- abrupt voiced/unvoiced changes;
- very low/high supported F0;
- empty/one-degree/max-degree custom tuning;
- arbitrary non-octave period extremes within supported bounds;
- automation jumps at block boundaries;
- dense automation over many blocks;
- zero-length/small/large legal host blocks;
- sample-rate changes;
- reset mid-phrase;
- preset/state restore mid-session;
- repeated prepare/reset cycles;
- long silence after signal to expose denormals/stale state;
- extreme but legal combinations of all deep controls.

## Perceptual tuning protocol

Do not optimize one universal score. Natural tuning, hard robotic correction, and deliberate destruction have different goals.

For each mechanism, create small blinded comparisons. Ask actionable questions:

- Which version would you use on this phrase?
- Which preserves the intended performance best?
- Which artifact is objectionable?
- Which extreme version is musically interesting rather than merely broken?

Rate only dimensions relevant to the mechanism, e.g. naturalness, intelligibility, transient integrity, pitch intent, identity/formant stability, artifact severity, character, preference.

Keep Nectar/other owned tools as references where useful, not as targets to clone.

## Sweet-spot mapping

For each important control, sweep the full safe range over representative fixtures and record candidate regions:

```
inaudible/subtle | useful-natural | useful-character | extreme-interesting | broken/unsafe
```

Then design the PLAY/ADVANCED mapping around the useful regions while DEEP retains access to valid extremes.

This should be done for at least:

- correction strength/speed;
- transition/portamento;
- vibrato retention/exaggeration;
- drift retention;
- formant shift/follow;
- intentional pitch shift magnitude;
- compressor density/attack behavior;
- saturation drive/character;
- periodic/aperiodic transform authority.

## Bug-proof control invariants

Candidate invariants for automated testing:

1. Every public parameter accepts its entire documented range without non-finite output.
2. Invalid external values sanitize deterministically.
3. 0% wet/correction/drive semantics are true or documented near-true bypasses.
4. No audio-thread allocation, lock, file/network I/O, logging, or unbounded work.
5. No parameter update can create an unbounded loop or resize a hot-path container.
6. Reset produces deterministic state independent of prior signal history.
7. Same input/state/settings produce deterministic output for deterministic modes.
8. Output safety remains authoritative under every creative setting.
9. Analysis failure cannot index tuning/history buffers out of bounds.
10. Structural changes have explicit handoff/crossfade behavior.
11. Deep controls cannot bypass sanitation or output safety.
12. Silence after arbitrary prior input converges to finite quiet output without pathological CPU state.

## What to build next vs what to defer

Build/refine now:

- explicit parameter sanitation and transition policy;
- shared analysis semantics;
- failure-degradation behavior;
- control ownership/composition;
- deterministic hostile-input fixtures;
- metrics needed for later perceptual sweeps;
- UI parameter metadata sufficient for PLAY/ADVANCED/DEEP exposure.

Do not spend the next wave on:

- new vocal feature families;
- elaborate new scale theory;
- giant UI redesign before DSP semantics stabilize;
- release claims;
- exhaustive listening before the required vocal mechanisms are integrated.

## Evidence boundary

This research establishes a design and validation strategy, not that Monkey's Ear currently sounds good, is easy to use, or is bug-proof. Those require implementation plus later listening/host evidence. The purpose of this document is to make those qualities first-class engineering targets rather than assumptions.