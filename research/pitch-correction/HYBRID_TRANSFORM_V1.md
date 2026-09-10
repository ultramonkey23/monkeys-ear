# Monkey's Ear — adaptive vocal transform experiment v1

Status: **SYNTHETIC research evidence only**. This file records one executable experiment. It does not promote a production renderer and does not replace `ARCHITECTURE.md` or real-vocal listening evidence.

## Question

Does a more capable time-domain path, an explicit phase-locked spectral path, or a simple adaptive combination show enough complementary behavior to justify a multi-representation vocal engine?

## Implemented candidates

### Expanded PSOLA-style baseline

The time-domain research path extends the earlier minimal baseline with:
- locally refined pitch marks around expected periods;
- multi-cycle grains;
- overlap normalization;
- polarity alignment against existing overlap;
- target-period grain placement.

It still receives known synthetic F0 and is not a complete production PSOLA implementation.

### Phase-locked spectral baseline

The spectral research path uses an STFT pitch-shift route with explicit peak-region phase locking. Spectral peaks advance using instantaneous-frequency phase accumulation and neighboring bins inherit the local peak's phase correction. Pitch shifting is implemented through time scaling followed by resampling.

This is a research implementation of identity-phase-locking-style behavior, not a claim of parity with mature commercial spectral engines.

### Adaptive hybrid v1

Both renderers remain independently available. A bounded routing weight is derived from:
- periodicity/harmonicity;
- requested pitch-shift magnitude;
- transient state;
- known synthetic noise amount.

The current policy favors the PSOLA path for highly periodic, smaller shifts and moves toward the spectral path as periodicity falls or shifts become large. The two outputs are combined with an equal-power-style crossfade. The routing law is deliberately simple so failures remain interpretable.

## Corpus

Six deterministic synthetic source/filter cases are used:
- steady voiced, +2 semitones;
- steady voiced, +5 semitones;
- steady voiced, +10 semitones;
- breathy voiced, +3 semitones;
- breathy voiced, +8 semitones;
- transient/noisy voiced, +5 semitones.

The source vowels use fixed synthetic /a/, /i/ and /u/-like formant envelopes. Each transformed render is compared with an oracle target synthesized directly at the requested F0 while keeping the same formant geometry.

## Recorded results

Across the six cases:

| Method | Mean envelope RMSE | Median envelope RMSE | Mean formant-peak error |
| --- | ---: | ---: | ---: |
| Adaptive hybrid v1 | **19.223 dB** | **18.196 dB** | 129.720 Hz |
| Expanded PSOLA | 21.027 dB | 20.466 dB | **47.469 Hz** |
| Phase-locked spectral | 20.888 dB | 19.921 dB | 162.382 Hz |

The hybrid obtained the best aggregate spectral-envelope error of the three, but it did **not** preserve formant peak locations as well as the expanded PSOLA path. Therefore the hybrid is not a general winner.

The routing behavior also exposed an important design problem. For the +10 semitone clean case the current rule assigns zero PSOLA weight and inherits the spectral path's roughly 162 Hz formant-peak error, even though the PSOLA baseline happened to retain the synthetic formant locations much better in that case. For noisy/breathy cases the spectral path greatly reduced broad envelope error while still moving formant peaks. This means representation routing cannot be based only on periodicity and pitch-shift magnitude.

## Interpretation

The experiment supports **multi-representation architecture**, but weakens the naive idea that one scalar blend weight can solve renderer selection.

The more promising design is to split responsibilities further:

1. use periodicity, detector confidence and source-type evidence to decide whether pitch-synchronous processing is trustworthy;
2. evaluate candidate renderers against explicit collateral-damage measures rather than hard-coding "large shift = spectral";
3. keep spectral-envelope/formant reconstruction independent from both renderers;
4. keep aperiodic/noise material on a separate path rather than relying on a voiced renderer to handle it;
5. allow region-level switching/crossfading while preserving one continuous target contour and recalled user intent.

In other words, the likely hybrid is a **planner plus specialized renderers**, not a fixed wet/dry mixture of PSOLA and a phase vocoder.

## Failure evidence retained

- Expanded PSOLA still produces relatively high broad envelope error on several synthetic cases despite strong formant-peak retention.
- Phase-locked spectral processing still moves formant peaks substantially in this experiment.
- The adaptive hybrid inherits spectral formant displacement whenever routing collapses fully toward the spectral path.
- The oracle target is synthetic and unusually well specified; real vocal identity, consonants, fry and transient behavior are not represented adequately.
- Known F0, synthetic noise amount and synthetic transient labels make the routing problem easier than a real plugin will face.
- No CPU, latency, automation, real-time safety or listening result exists here.

## Next executable gate

Do not add another fixed hybrid law. The next experiment should introduce a **candidate renderer planner**. For every short region, generate at least PSOLA and phase-locked spectral candidates, apply the same independent cepstral/LPC envelope reconstruction candidates, then score measurable collateral damage before selecting or crossfading. The planner should be tested with a time-varying expression-preserving target contour and should include explicit aperiodic/unvoiced bypass or recombination.

Promotion remains blocked until real-vocal same-source renders and controlled listening exist.
