# Monkey's Ear pitch correction — research history

Status: concise evidence ledger. Keep active design in `ARCHITECTURE.md`; keep this file for what was learned, rejected, or still needs proof.

## 2026-09-07 — research initialization

### Repo/source-truth correction

The GitHub repository was created after a more complete local Monkey's Ear implementation already existed. Therefore GitHub absence is not evidence of product absence. Pitch research remains pre-integration until local code is imported/audited.

### Lab knowledge reconciled

Useful existing Lab audio knowledge that should carry into Monkey's Ear rather than be reinvented:
- REAPER is the preferred human audio workbench/test surface, not the product architecture.
- Prefer small framework-independent deterministic C++ cores; CLAP/iPlug2 are wrapper candidates, not mandatory internals.
- Code Prime remains the intentional mutation boundary; audio specialists are capabilities rather than a second authority.
- Existing Savage Crown Living Audio Forge is prior art for deterministic WAV inspection/transforms, source provenance, native math-sound work and runtime seams.
- Real creative approval remains human-in-the-loop; synthetic metrics do not replace listening.

### External baseline research

**MAutoPitch** is useful as a simple automatic correction benchmark. Its documented controls include correction behavior, pitch-detector configuration, scale selection and formant handling. This is a baseline Monkey's Ear must match or exceed for basic usability, not a design to copy.

**Melodyne** is a stronger decomposition benchmark. Its documentation treats pitch center, pitch modulation and pitch drift as independently editable concepts and gives special handling to sibilant/unvoiced material so those components are not simply transposed with voiced pitch content.

**YIN** remains a strong classic detector baseline: speech/music F0 estimation with relatively few parameters and efficient/low-latency implementation characteristics.

**WORLD** is valuable as a research/reference transform because it explicitly estimates F0, spectral envelope and aperiodicity and can resynthesize from those streams. This separation is useful for testing whether pitch correction is damaging timbre/noise structure, but WORLD is not promoted as the production engine.

### Current hypothesis

The strongest current direction is **expression-preserving pitch correction**:
- correct note center separately from slow drift;
- preserve or deliberately scale rapid modulation/vibrato;
- preserve/control transitions and portamento separately from note center;
- protect unvoiced consonants, sibilants and breath from ordinary harmonic pitch shifting;
- preserve formant/spectral-envelope identity unless the user asks for a creative shift;
- keep detector, musical target, candidate contour and transformation engine replaceable.

### Shared test-corpus decision

Use the same immutable dry recorded vocal as the primary A/B source for Monkey's Ear and every external correction tool Cody owns. Preserve exact settings and rendered WAVs. The selected corrected render becomes an additional standardized source for testing later EQ, dynamics, saturation, modulation and spatial modules while retaining the original dry master.

See `../TEST_PROTOCOL.md`.

## 2026-09-07 — detector baseline v1

Evidence state: **SYNTHETIC**. This is executable detector evidence, not product or listening proof.

Added `experiments/detector_baseline_v1.py` and its recorded CSV output. The experiment compares a straightforward YIN implementation with an NSDF/MPM-style implementation on six generated monophonic cases: stable pitch, vibrato, slide, strong second harmonic, noisy pitched content and breathy pitched content.

Results:
- MPM produced lower median absolute cents error than YIN on the stable, vibrato, slide and moderate-noise cases in this particular synthetic setup.
- YIN remained robust on the deliberately adversarial strong-second-harmonic case, while the current MPM peak-selection rule locked one octave high: approximately 1203 cents median error and a 100% octave-error rate.
- YIN also substantially outperformed the current MPM implementation on the breathier/noisier case.
- Therefore neither detector is promoted as the universal production detector. MPM remains useful as an adversarial/secondary baseline, while YIN currently has the stronger failure behavior across this small corpus.
- The main research opportunity is not to choose a winner prematurely, but to separate raw candidate generation from temporal/octave decision logic so strong harmonic traps can be rejected without giving up MPM's precision on easier frames.

This result supports the existing replaceable-detector architecture and gives the next experiment a concrete target: candidate-level comparison plus temporal continuity/hysteresis on octave traps and voiced/unvoiced transitions.

## 2026-09-07 — contour decomposition v1

Evidence state: **SYNTHETIC**. This tests pitch-contour control logic only; no audio was transformed and no listening conclusion is allowed.

Added `experiments/contour_decomposition_v1.py` plus a recorded aggregate summary. The deterministic Monte Carlo corpus contains 200 three-note phrases with randomized note intervals, center offsets, slow drift, vibrato rate/depth and 100–240 ms transition regions.

The component estimator uses only the observed contour plus a chosen musical target trajectory. It estimates a slow residual with a 205 ms smoothing window, derives per-note center from stable note regions, treats the remaining slow component as drift and preserves the fast residual as modulation/vibrato. The requested experimental behavior is 100% center correction, 70% drift correction and 95% fast-modulation retention.

Aggregate results over 200 phrases:
- **component estimator v1:** mean 3.548 cents RMSE against the requested corrected component mix; worst case 6.002 cents; mean center residual 0.456 cents.
- **hard snap:** mean 40.730 cents RMSE; worst case 80.886 cents.
- **75% whole-contour pull:** mean 31.111 cents RMSE; worst case 62.113 cents.
- Fast-modulation retention was 1.138 for the component estimator versus 0.238 for hard snap and 0.336 for whole-contour pull. The estimator slightly over-retained/contaminated the fast band, so the current 205 ms decomposition is useful but not finished.
- Transition-region RMSE remained 24.652 cents for the component estimator, much lower than the two naive baselines but still too high to claim transparent transition preservation.

Interpretation: separating center, drift, fast modulation and target transitions is strongly supported as an architecture direction in this synthetic contour model. The specific smoothing/decomposition constants are not promoted; transition isolation and vibrato-band leakage need further work.

## 2026-09-07 — source/filter formant preservation v1

Evidence state: **SYNTHETIC / ORACLE ENVELOPE**. This isolates spectral-envelope geometry and is intentionally easier than real formant estimation/resynthesis.

Added `experiments/formant_preservation_v1.py` and recorded results for a synthetic three-formant vocal-tract envelope. Fundamental frequencies of 110, 180 and 260 Hz were shifted by -7, +5 and +12 semitones.

When harmonic amplitudes remained attached to harmonic index as pitch moved, the effective formant envelope moved with pitch and produced 7.137–8.971 dB log-envelope RMSE against the original vocal-tract target, with a mean of approximately 7.909 dB across the nine cases.

When amplitudes were re-evaluated from the original envelope at the shifted harmonic frequencies, envelope error was zero by construction. This is an oracle/reference result, not evidence that True Envelope, LPC or any practical estimator can achieve zero error.

Interpretation: formant/timbre preservation must remain structurally independent from pitch transposition. The next practical comparison should replace the oracle envelope with estimated LPC and cepstral/True-Envelope-style envelopes on the same source, then compare actual resynthesis families.

## 2026-09-07 — resynthesis + practical envelope estimator v1

Evidence state: **SYNTHETIC / FAILURE-PRESERVING BASELINE**. Added `experiments/resynthesis_formant_v1.py`, `resynthesis_v1_results.csv` and `envelope_estimator_v1_results.csv`.

The resynthesis experiment uses deterministic synthetic /a/, /i/ and /u/-like source/filter vowels at 120 and 220 Hz, shifted by -5, +5 and +12 semitones. Both transformation families receive the same source and target ratio. The time-domain path is a deliberately minimal known-F0 PSOLA-style baseline. The frequency-domain path uses librosa's phase-vocoder-based pitch shifter as an external algorithmic baseline. Neither is promoted as production DSP.

Against an oracle formant-preserved synthetic reference:
- **PSOLA baseline:** median smoothed spectral-envelope RMSE 21.737 dB; mean formant-peak error 46.279 Hz.
- **phase-vocoder baseline:** median smoothed spectral-envelope RMSE 18.528 dB; mean formant-peak error 158.022 Hz.

The tradeoff is important: this crude PSOLA path often kept formant peak locations closer while producing larger overall envelope-shape error on many upward/high-F0 cases. The phase-vocoder baseline often reduced global envelope RMSE relative to the crude PSOLA path but moved formant peaks much more. The spread is large across conditions, so neither family wins and neither implementation should be integrated from this evidence.

The same experiment replaced the previous oracle-only envelope assumption with practical estimators. Across /a/, /i/, /u/, F0 values 110/220/350 Hz and clean plus low-noise conditions:
- **CEP40:** mean 3.471 dB, median 3.760 dB, max 4.736 dB log-envelope RMSE.
- **LPC20:** mean 4.583 dB, median 4.465 dB, max 5.833 dB.
- **CEP20:** mean 4.794 dB, median 4.934 dB, max 7.130 dB.
- **LPC12:** mean 5.820 dB, median 6.130 dB, max 7.207 dB.

In this synthetic corpus, a 40-coefficient cepstral envelope is the strongest of the four tested estimators, including the high-F0 subset. That is not yet a True Envelope implementation and does not prove superiority on real vocals. It does justify carrying a cepstral/True-Envelope-style candidate forward alongside LPC rather than treating LPC as the default.

The experiment also writes four local synthetic WAV references when run: dry source, oracle formant-preserved +7 semitone reference, PSOLA-style +7 render and phase-vocoder +7 render. These are synthetic listening aids only and are not committed as production assets.

Interpretation: gate 4 has now started with executable transformation evidence. The result does not identify a winner; instead it makes the next rewrite target clearer. The next transformation prototype should explicitly combine pitch transformation with independent envelope reconstruction, and phase locking/transient handling must be tested directly rather than inferred from a generic phase-vocoder baseline.

## Not yet proven

- quality of any Monkey's Ear production pitch detector or resynthesis algorithm;
- whether the local implementation already contains equivalent/better mechanisms;
- real-time CPU/latency behavior;
- F0 tracking robustness on Cody's real voice;
- practical formant-envelope estimation quality on real voice;
- sibilant/unvoiced segmentation quality;
- listening preference versus MAutoPitch, Melodyne or other available tools;
- whether the synthetic contour advantage survives detector errors and real vocal resynthesis;
- whether a proper production PSOLA or phase-locked spectral implementation materially outperforms these research baselines.

## Next evidence

The next executable work should connect these pieces instead of adding more isolated prose:
1. add candidate-level octave continuity/hysteresis and voiced/unvoiced transitions to detector testing;
2. improve contour transition isolation and fast-band leakage on the randomized corpus;
3. implement independent envelope reconstruction around both transformation families, carrying LPC20 and cepstral/True-Envelope-style candidates;
4. replace the generic frequency-domain baseline with an explicit phase-locked spectral prototype and add transient/onset tests;
5. drive both transforms from the same time-varying target contour rather than constant semitone shifts;
6. move to the same immutable real vocal and external-tool comparison before promoting any mechanism beyond synthetic evidence.
