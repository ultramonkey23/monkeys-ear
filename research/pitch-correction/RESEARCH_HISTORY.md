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

## Not yet proven

- quality of any Monkey's Ear production pitch detector or resynthesis algorithm;
- whether the local implementation already contains equivalent/better mechanisms;
- real-time CPU/latency behavior;
- F0 tracking robustness on Cody's real voice;
- formant preservation quality;
- sibilant/unvoiced segmentation quality;
- listening preference versus MAutoPitch, Melodyne or other available tools;
- whether center/drift/modulation separation provides a meaningful audible win in the final product.

## Next evidence

The next durable evidence should remain executable: extend detector tests to candidate-level octave handling and voiced/unvoiced transitions, then move into contour decomposition and same-source resynthesis comparisons. Real vocal renders remain required before any detector or transformation family is promoted beyond synthetic evidence.
