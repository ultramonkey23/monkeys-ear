# Monkey's Ear pitch correction — pre-integration architecture

Status: active research guidance only. The local Monkey's Ear implementation has not yet been imported/audited into this GitHub repository. This file defines research hypotheses and test boundaries, not final product DSP.

## Goal

Correct pitch without assuming that all pitch motion is an error. Preserve useful expression, note transitions, timbre, consonants/noise and performance identity while giving the user explicit control over how much of each is corrected.

The target is not an Auto-Tune clone. The system should be strong as an ordinary tuner first, then add deeper control only where it produces a measurable or audible win.

## Core decomposition

Keep these responsibilities independent:

- **Pitch evidence** — F0 candidates, confidence, voiced/unvoiced state, onset/offset evidence and temporal continuity.
- **Musical intent** — scale/key/chord/note targets, free pitch, user-drawn targets, MIDI/automation targets where available.
- **Expression model** — note center, slow drift, intentional modulation/vibrato, transitions/portamento and optional timing relationships.
- **Source-type evidence** — pitched content versus unvoiced/sibilant/breath/noise-like content; confidence must be allowed to become unknown.
- **Candidate correction** — proposed target trajectory and correction amount before audio transformation.
- **Bounds/projection** — reject or reduce changes that violate user limits, create implausible discontinuities, damage protected expression, or exceed latency/mode constraints.
- **Resynthesis / transformation** — the actual pitch modification engine. Detection and target logic must not be hard-wired to one resynthesis algorithm.
- **History** — note continuity and previous decisions only where they improve tracking/correction; avoid hidden state that cannot be recalled.

## Baselines to beat

### Simple automatic correction

MAutoPitch is a useful baseline because it exposes the expected workhorse controls: correction depth/speed, detector settings, scale selection and formant handling. Monkey's Ear must be able to perform this basic job before claiming deeper intelligence.

### Detailed musical editing

Melodyne-class behavior is a stronger benchmark for research because pitch center, rapid modulation/vibrato and slower drift can be manipulated independently, and its melodic workflow treats sibilants/unvoiced material differently from pitched content. Monkey's Ear should benchmark this separation rather than flattening every contour toward a note center.

## Detection candidates

Do not lock product architecture to one detector before the local-source audit.

Research baselines:
- **YIN** — classic low-parameter F0 estimator suitable for speech/music and efficient implementation.
- probabilistic/temporal variants such as **pYIN-style tracking** — useful benchmark for confidence and sequence continuity.
- alternative spectral/cepstral/autocorrelation approaches — required as adversarial baselines for source types where YIN-like methods struggle.

The detector must be swappable. Pitch correction quality cannot exceed the quality of voiced/unvoiced decisions and F0 continuity.

## Transformation / resynthesis candidates

Do not confuse pitch detection with pitch shifting.

Research families:
- pitch-synchronous/time-domain methods such as PSOLA-style processing for a classic vocal baseline;
- phase-vocoder / frequency-domain approaches where their artifact/latency tradeoffs are appropriate;
- source/filter decompositions for offline/reference testing;
- **WORLD** as an important research/reference path because it explicitly represents F0, spectral envelope and aperiodicity separately.

WORLD or any other vocoder is not automatically the production choice. It is useful because it lets experiments isolate pitch trajectory from spectral envelope and noise/aperiodic content.

## Expression-preserving correction

The strongest current research direction is to represent correction as multiple controllable components rather than one snap amount:

1. **Center** — move the average/stable pitch of a note toward the intended target.
2. **Drift** — optionally reduce slow unintended wandering independently of center.
3. **Modulation** — preserve, scale or deliberately reshape vibrato/rapid pitch variation.
4. **Transition** — preserve or control slides, scoops and note-to-note motion instead of quantizing them blindly.
5. **Unvoiced protection** — consonants, sibilants, breath and other noise-like content should normally avoid harmonic pitch shifting unless deliberately targeted as an effect.
6. **Formant/timbre protection** — pitch movement should not automatically drag the spectral envelope in a way that changes perceived vocal identity.

The user may still request extreme/robotic correction. Naturalness is not mandatory; unintended damage is.

## Proposed control surface

Normal use should remain small:
- target key/scale or chromatic;
- correction amount;
- response/speed;
- formant/timbre preserve state;
- bypass.

Advanced expansion may expose:
- center correction;
- drift correction;
- vibrato/modulation preservation or scaling;
- transition/portamento behavior;
- voiced/unvoiced sensitivity;
- minimum/maximum F0;
- note-change hysteresis;
- target source (scale/MIDI/manual/free);
- latency/quality mode.

Do not surface every analysis parameter merely because it exists internally.

## Live / Studio / Render

### LIVE
Prioritize bounded CPU, predictable latency, stable F0 tracking and artifact avoidance. No noncausal correction without explicit lookahead latency.

### STUDIO
Allow more history, higher-quality analysis and optional manual target editing while remaining interactive.

### RENDER
Permit offline/pre-analysis and heavier resynthesis if it wins quality tests and remains reproducible.

The same saved musical intent should survive mode changes even if the underlying algorithm differs.

## Cross-module role

Pitch correction is also a test-source generator. The same dry vocal should produce versioned pitch-corrected references that can then be fed through EQ, compression, saturation, modulation and spatial modules. This makes pitch quality failures traceable instead of contaminating every later test without provenance.

Follow `../TEST_PROTOCOL.md` for the shared corpus and A/B procedure.

## Current-practice evidence

- Celemony documents separate pitch center, modulation and drift editing, plus special handling for sibilants/unvoiced material.
- MAutoPitch provides a useful simple automatic baseline with detector, correction and formant controls.
- YIN remains a classic efficient F0-estimation baseline for speech and music.
- WORLD separates F0, spectral envelope and aperiodicity, making it valuable for research decomposition and offline reference experiments.

## What not to assume

- Do not assume GitHub absence means the local product lacks pitch code.
- Do not assume one F0 detector works for every source.
- Do not treat vibrato as pitch error by default.
- Do not pitch-shift all unvoiced content by default.
- Do not treat formant preservation as optional polish; it is part of identity preservation for natural vocal correction.
- Do not hide lookahead or latency.
- Do not add ML because it sounds modern; deterministic baselines must remain available and authoritative.

## First research gates

1. Build a synthetic F0/voicing corpus with stable notes, drift, vibrato, slides, octave traps, breath/noise and abrupt transitions.
2. Compare at least two F0 estimation strategies and log confidence/octave/continuity failures.
3. Separate center/drift/modulation mathematically on known synthetic contours before transforming audio.
4. Compare at least two transformation families on identical target contours.
5. Run Cody's same recorded vocal through Monkey's Ear research prototype and external correction tools using `../TEST_PROTOCOL.md`.
6. Preserve failures as a regression corpus.
7. Only after the local implementation is imported, reconcile this research with existing code rather than starting a parallel rewrite.

## Promotion rule

A new mechanism survives only if it beats a simpler baseline on a defined engineering tradeoff, wins level-matched listening for a defined task, or provides a distinct user-controlled creative effect. Otherwise demote it and keep only the evidence needed to prevent rediscovery.
