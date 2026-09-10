# Monkey's Ear Vocal — Open-Source DSP Design Mining

Date: 2026-09-10
Status: research/design input, not listening proof
Scope: evolve the already-frozen vocal feature set; no new feature families.

## Why this pass exists

This pass inspected open-source implementations and compared their mechanisms against the current Monkey's Ear vocal owner. The goal is not to copy libraries or turn Monkey's Ear into a wrapper. It is to extract mechanisms, failure modes, test ideas, and architecture that improve our own bounded real-time implementation.

## Current Monkey's Ear observations

At production commit 826babe, `AudioInputProcessor::process_vocal_sample()` already contains useful seeds:

- confidence-weighted periodic/mixed/aperiodic estimate;
- fast/slow pitch state;
- onset estimate;
- articulation-aware `VocalTargetEngine`;
- causal two-grain primary PSOLA;
- optional softer second PSOLA pass;
- broad energy-envelope repair;
- bounded high-frequency residual reinjection;
- simple `fast_tanh` character stage.

The strongest design correction from this research is that several of those seeds should be separated into explicit shared analysis and transformation responsibilities rather than continuing to grow inside one sample function.

## Open-source mechanisms worth mining

### 1. Signalsmith Stretch — architectural reference, not LIVE drop-in

Signalsmith Stretch is MIT-licensed C++ and exposes explicit input/output latency, configurable block/interval sizes, split computation, arbitrary monotonic frequency maps, formant factor, pitch-compensated formant handling, and a rough formant-base/F0 input. Its default configuration uses roughly 120 ms analysis blocks and 30 ms intervals; its cheaper preset is roughly 100/40 ms. That makes it a strong STUDIO/RENDER reference but not a direct answer for Monkey's Ear's ultra-low-latency LIVE path.

Design lessons:

1. Treat latency as an explicit algorithm property, never a hidden side effect.
2. Keep frequency mapping general rather than hardcoding semitone transposition.
3. Formant transform and pitch transform should be independently addressable but able to compensate one another.
4. Split-computation is useful: expensive spectral work can be distributed over time instead of spiking on one audio callback.
5. Silence handling and reset/seek behavior deserve explicit state-machine treatment.

Monkey's Ear consequence: keep PSOLA as LIVE primary, but define a transform interface broad enough that STUDIO/RENDER can later use a spectral/reference implementation without changing musical semantics.

### 2. stftPitchShift — source/envelope/residual decomposition

The project describes formants as a smoothed spectral envelope extracted by cepstral low-pass liftering. It removes the envelope, pitch-shifts the remaining excitation/residual, then recombines the preserved envelope.

This is a much better conceptual model than Monkey's Ear's current scalar broad-energy repair. Current `source_envelope_ / shifted_envelope_` can only restore overall amplitude/body; it cannot preserve the shape or locations of vocal-tract resonances.

Recommended evolution:

- keep current scalar repair as cheap LIVE fallback;
- add a bounded spectral-envelope representation as shared analysis state;
- compare cepstral liftering and LPC envelope estimation on a deterministic vowel corpus;
- transform excitation separately from envelope;
- allow envelope map to remain stationary, partially follow pitch, shift, or stretch;
- never claim formant preservation from scalar RMS/envelope matching.

### 3. audiojs/shift — useful failure taxonomy

Its PSOLA documentation is unusually useful because it states what PSOLA destroys as well as what it preserves. Pitch-synchronous grains work well on voiced monophonic material but pitch-mark jitter hurts non-periodic attacks; unreliable periods need fallback behavior; resampling-based variants move formants unless separately compensated.

Monkey's Ear already has periodic/aperiodic blending, so evolve that into transform confidence rather than a binary voiced gate:

```
transform_authority = pitch_confidence
                    * periodicity
                    * phrase_stability
                    * (1 - transient_protection)
```

Do not use this exact equation as gospel; test it as a bounded policy. The key idea is that PSOLA authority should decrease continuously when the signal stops looking like the material PSOLA handles well.

### 4. ChowDSP ADAA — saturation architecture

ChowDSP's waveshaper module contains integrated/ADAA hard, tanh, soft clipping and rectification. Its tanh implementation explicitly documents one sample of latency and initializes the nonlinearity plus first and second antiderivatives.

Lessons:

- anti-alias strategy belongs to the waveshaper contract;
- latency introduced by anti-aliasing must be explicit;
- the nonlinearity and its antiderivatives can be separated from the generic ADAA processor;
- do not assume oversampling is the only route to cleaner nonlinear DSP.

Monkey's Ear consequence: do not merely replace `fast_tanh` with a giant oversampled saturator. Build a small saturation kernel interface where LIVE can choose a low-cost antialiased strategy and STUDIO/RENDER can spend more CPU. Because the current product contract prefers zero reported latency, one-sample ADAA latency must be evaluated explicitly rather than silently adopted.

### 5. JUCE compressor — useful baseline precisely because it is simple

JUCE's standard compressor exposes the conventional threshold/ratio/attack/release model. That is a useful control case for Monkey's Ear. Our novelty should not be a bizarre gain computer; it should be steering a stable compressor from shared vocal evidence.

Recommended two-path design:

- Safety/peak path: fast, conventional, bounded.
- Density/body path: slower, phrase-aware leveling.
- Combine requested gain reduction conservatively; do not sum dB blindly without bounds.
- Vocal state may steer attack/release/ratio/threshold within fixed ranges, never allocate or redesign topology in the audio callback.

## Architecture changes recommended

### A. Shared analysis should become authoritative

Move toward one fixed-memory `VocalAnalysisFrame` produced once and consumed by target selection, pitch transform, formant engine, dynamics and saturation.

Minimum useful semantic fields:

```
f0_cents
pitch_confidence
center_cents
drift_cents
vibrato_cents
vibrato_rate_hz
transition_cents
pitch_residual_cents
periodicity
aperiodicity
transient_probability
onset_probability
release_probability
connection
stability
rms
peak
spectral_flux
```

A small fixed spectral-envelope representation can be added only after the implementation experiment proves the cost/benefit.

### B. Replace the current fast/slow pitch trick with a filter bank, not one magic time constant

Current code uses ~8 ms and ~110 ms one-pole states and labels their difference vibrato. That difference also contains slides, overshoot, detector jitter and other motion.

Prototype a causal multi-timescale bank, for example fast/medium/slow states, then classify the differences using periodicity and temporal consistency. The important change is semantic: `fast - slow` is motion evidence, not automatically vibrato.

Candidate decomposition:

- center: robust slow trajectory conditioned on note/phrase state;
- drift: very-low-frequency deviation from target/center;
- vibrato: quasi-periodic residual with bounded plausible rate;
- transition: monotonic/coherent pitch motion during connected note change;
- residual: everything not confidently explained.

### C. Target engine should consume uncertainty

Current target scoring is deterministic once observed cents/motion are supplied. Feed it analysis confidence and state so candidate advantage must overcome uncertainty-dependent switching cost.

Suggested scoring decomposition for experiments:

```
score = destination_affinity
      + directional_affinity
      + path_affinity
      + continuity_affinity
      - distance_cost
      - switch_cost
      - transient_cost
```

Keep these terms individually inspectable in metrics. Do not bury them in one opaque learned scalar.

### D. Separate degree semantics

Do not let one `gravity` number simultaneously mean attraction, capture radius, destination importance and path importance.

Prototype fixed fields such as:

```
destination_strength
capture_strength
passing_strength
ascending_affinity
descending_affinity
release_resistance
```

The public UI may collapse them into simple musical roles (Anchor/Stable/Color/Passing/Approach/Avoid), but the DSP should not confuse those meanings.

### E. Transform policy should be confidence-weighted

Use the same target trajectory but vary transform authority:

- stable periodic vowel: high PSOLA authority;
- mixed voiced consonant: lower PSOLA + more protected residual;
- transient/sibilant: near bypass of harmonic transform;
- uncertain F0: preserve source rather than hallucinate certainty.

This evolves the existing periodic/aperiodic mechanism rather than replacing it.

### F. Formant path should have tiers

LIVE cheap tier:
- current broad envelope repair, improved and honestly named `body_repair` rather than formant preservation.

LIVE+ / STUDIO candidate:
- small LPC envelope or reduced spectral envelope.

STUDIO/RENDER candidate:
- cepstral spectral-envelope extraction and independent remapping.

All tiers should obey the same semantic controls: preserve/follow/shift/stretch/identity.

### G. Saturation should consume source decomposition

Instead of `fast_tanh(dual_layer * drive)` over the whole reconstructed signal:

- periodic body gets primary harmonic enrichment;
- protected aperiodic/transient component gets less or none;
- optional asymmetry creates even harmonics;
- drive can rise during stable vowel body and relax at consonant attack;
- output trim and limiter remain independent.

Do not create discontinuous waveshaper switching from frame to frame. Interpolate coefficients/drive continuously.

### H. Dynamics should consume phrase state

A phrase-aware compressor can remain deterministic:

```
attack_ms  = map(onset/transient evidence within bounded range)
release_ms = map(connection/release evidence within bounded range)
body_target = map(stability/periodicity within bounded range)
```

Parameter steering must be smoothed. Musical analysis is advisory; safety path remains authoritative for dangerous peaks.

## Implementation experiments, in order

1. Refactor analysis evidence out of `process_vocal_sample()` without changing sound. Prove bitwise/near-bitwise behavior where practical.
2. Add multi-timescale pitch-motion metrics; do not change transform yet.
3. Classify vibrato vs transition vs residual on synthetic contours and recorded test fixtures when available.
4. Feed confidence/state into target switching and measure chatter/passing-tone capture.
5. Add independent transpose after musical target computation but before transform ratio planning.
6. Rename scalar `formant_repair` semantics internally if necessary; compare LPC vs cepstral envelope preservation offline first.
7. Build two-path dynamics prototype driven by shared analysis.
8. Build saturation kernel benchmark: current `fast_tanh`, oversampled tanh, first/second-order ADAA where compatible, asymmetric soft curve. Measure alias energy, CPU and latency.
9. Only then integrate the winning bounded mechanisms into LIVE.

## Benchmark dimensions to steal, not code

The open-source time/pitch benchmark ecosystem suggests measuring more than pitch error. Monkey's Ear should eventually track:

- F0 error;
- formant/spectral-envelope distance;
- transient correlation;
- phase coherence;
- alias energy;
- CPU cost;
- algorithmic latency;
- discontinuity/seam metrics;
- voiced/unvoiced damage;
- target chatter/switch count.

No single composite score should become truth. Keep a Pareto view because natural correction, hard robotic tuning and extreme character intentionally optimize different things.

## Licensing discipline

Mechanisms and research ideas can be learned from all inspected projects, but source-code reuse must respect licenses. In particular, Signalsmith Stretch is MIT; ChowDSP modules have mixed licenses and the waveshaper module is GPLv3; JUCE has its own commercial/AGPL licensing; Rubber Band is GPL/commercial. Do not paste GPL/AGPL code into Monkey's Ear unless the product licensing decision explicitly permits it. Prefer clean-room implementation from published algorithms and our own tests where licensing is incompatible.

## Strongest design conclusions

1. Current scalar envelope repair is body repair, not true formant preservation.
2. Current `fast - slow` value is motion residual, not reliable vibrato by itself.
3. PSOLA should have continuously varying authority based on source suitability.
4. One shared analysis frame should drive musical targeting, transformation, dynamics and saturation.
5. Degree gravity must split into destination/path/capture semantics before scale behavior becomes truly expressive.
6. LIVE, STUDIO and RENDER should share musical intent but are allowed different transform engines and analysis depth.
7. Saturation should be antialias-aware and source-aware, not just a final `tanh` color knob.
8. Compressor novelty should come from performance-aware control, not unstable compressor mathematics.
9. Latency is part of every DSP mechanism's contract.
10. After these mechanisms converge and are perceptually tuned, freeze vocal and return to synth development.

## Evidence boundary

This document is architecture/research guidance from code inspection. It does not establish listening quality, DAW behavior, physical latency, or superiority to commercial products. Any code inspired by these mechanisms must still pass Monkey's Ear's RT, deterministic, build, and later listening gates.