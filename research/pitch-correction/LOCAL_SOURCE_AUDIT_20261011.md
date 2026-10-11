# Pitch-correction local source audit — 2026-10-11

Evidence state: MEASURED (source inspection only — no audio quality or usability claim).
Source truth: local product code at commit `c6d2824` (`source_truth_order` rung 2: local_product_code_and_live_runtime).
Scope: answers the standing `PENDING_LOCAL_AUDIT` for `pitch-correction` in `research/STATUS.json`
and the open question in `RESEARCH_HISTORY.md` ("whether the local implementation already
contains equivalent/better mechanisms"). This audit does **not** promote any mechanism; it maps
product source against the decomposition in `ARCHITECTURE.md` so research experiments target the
real gaps instead of re-inventing existing product surface.

## Verdict in one paragraph

The product already implements the *candidate correction* and *resynthesis* layers end-to-end:
a tuning-space target engine (`VocalTargetEngine`) plus a two-stage pitch-synchronous
grain/PSOLA-style shifter with envelope repair, residual injection, and aperiodic protection
(`AudioInputProcessor::process_vocal_sample`). The `ARCHITECTURE.md` responsibilities that are
real in source: musical intent (rich), expression split (drift/vibrato retention), source-type
evidence (three-state model), resynthesis (PSOLA-style baseline family), history (explicit
target hysteresis). The real gaps are in *pitch evidence*: the detector surface is a single
Hz + confidence pair with a hard voiced threshold — no candidate octave model, no V/UV
probability, no onset evidence fed to the target engine from the tracker. Research "next
evidence" items 1 and 6 remain the correct next steps; the product does not contain a competing
detector architecture that would invalidate them.

## Decomposition mapping (ARCHITECTURE.md §Core decomposition)

| Responsibility | Product state | Source citations |
| --- | --- | --- |
| Pitch evidence | PARTIAL — single F0 + confidence | `ExternalSubharmonic` tracker surface `tonal.h:65-88`; local authority `vocal_module.cpp:72-85`; voiced gate `audio_input.cpp:12` (`tracked_hz∈[55,900] && conf>.35`) |
| Musical intent | EXISTS — tuning spaces + target engine | `TuningSpace` 30 builtin tunings + custom degrees w/ gravity & direction (`vocal_target.h:31`, `vocal_target.cpp:5-33`); candidate search ±2 octaves (`vocal_target.cpp:47`); no MIDI/automation targets (`accepts_midi=false`, `vocal_module.cpp:24`) |
| Expression model | EXISTS | drift/vibrato split via slow-centroid residual retention (`audio_input.cpp:16`); transition smoothing `tm=2+(1-transition)*58` ms (`audio_input.cpp:16`); glide trajectory `glide_ms=3+preserve*(12+280*connected)+portamento*420*connected` (`vocal_target.cpp:53`) |
| Source-type evidence | EXISTS (coarse) | `VocalSourceType {Aperiodic,Mixed,Periodic}` + metrics (`audio_input.h:9-10`); `aperiodic_protection` gates pitched-path mix (`audio_input.cpp:17`); no sibilant/breath segmentation |
| Candidate correction | EXISTS | scoring `gravity − |candidate−observed|/capture`, capture `38+155·gravity`, directionality lerp (`vocal_target.cpp:37-39`); onset protection w/ articulation (`vocal_target.cpp:50`); metrics exposed (`audio_input.h:10`) |
| Bounds/projection | EXISTS | correction clamp ±2400 ¢ (`audio_input.cpp:16`); period clamp 53–872 samples (`audio_input.cpp:16`); all controls sanitized (`audio_input.cpp:5`); corrupt tuning degrades to neutral, not stale latch (`vocal_target.cpp:48-49`) |
| Resynthesis / transformation | EXISTS (PSOLA-style family) | dual sine²-windowed overlapping grains, two stages blended by `sequential_stage_mix` (`audio_input.cpp:16`); envelope-based `formant_repair` gain clamp 0.65–1.55 (`audio_input.cpp:17`); 1700 Hz residual path `spectral_residual_mix` (`audio_input.cpp:17`); `character` tanh coloration (`audio_input.cpp:17`); plugin mix contract keeps inner processor fully wet (`vocal_module.cpp:38-44`) |
| History | EXISTS (minimal, explicit) | previous degree/target hysteresis `history=(1−detached)*(0.22+0.62·hysteresis)` (`vocal_target.cpp:51`); switch counting (`vocal_target.cpp:52`); no hidden note memory beyond these fields |

## Evidence-authority chain (worth preserving)

`VocalModule` resolves pitch authority monotonically by confidence, never by ambient priority:
local causal tracker → manual evidence (`set_local_pitch_evidence`, `vocal_module.cpp:52-56`) →
shared ecosystem evidence (`consume_ecosystem`, `vocal_module.cpp:58-66`), where shared evidence
seizes authority only when `shared_confidence >= local_confidence` (`vocal_module.cpp:79-85`).
Mid/side split keeps the shifter on the mid channel (`vocal_module.cpp:72-92`), and non-finite
outputs fall back to dry with a health flag (`vocal_module.cpp:90`). This matches the
ARCHITECTURE rule that detection/target logic must not be hard-wired to one resynthesis
algorithm: the tracker is swappable (`external_sub_` in `engine.cpp:332` feeds a second tracker
instance into the same correction path).

## Control smoothing / real-time behavior (engineering observation)

All ten expression controls glide on a 5 ms one-pole (`audio_input.cpp:7`), so parameter jumps
cannot step the correction path. Target trajectory smoothing is separate (`vocal_target.cpp:53`).
No CPU/latency claim is made here; the on-device run of `monkeys_ear_test_runner` (margins
measured 2026-10-11) covers block-deadline behavior of this path at 32–256 sample blocks.

## Gap analysis vs research hypotheses

1. **Detector candidates (ARCHITECTURE §Detection candidates) — NOT IN PRODUCT.** Product
   exposes one `tracked_frequency_hz()+confidence()`. No YIN/pYIN-style candidate set, no
   octave-continuity model. Confirms research "next evidence" item 1 (candidate-level octave
   continuity/hysteresis and V/UV transitions in detector testing) is still the right next
   experiment — it is not redundant with product code.
2. **Hard voiced threshold.** `conf>.35` (`audio_input.cpp:12`) is a gate, not a probability;
   `voiced_mix` output suggests a soft blend exists internally but the shift path keys off the
   binary. Any future V/UV probability model should land here first.
3. **Resynthesis family.** Product is the pitch-synchronous/time-domain baseline family the
   research calls for ("PSOLA-style processing for a classic vocal baseline") plus a
   phase-*unlocked* spectral residual approximation (1700 Hz one-pole). The research's
   phase-locked spectral / WORLD reference paths are NOT present; experiments 4-5 in the
   research "Next evidence" list therefore remain open, and the product two-stage grain chain is
   the correct in-product baseline to beat.
4. **MIDI/automation targets — NOT PRESENT** (`accepts_midi=false`). Fine for a vocal FX first;
   note it before designing user-drawn target surfaces.
5. **Correction decomposition matches the Melodyne-benchmark direction in part:** drift and
   vibrato are retained independently (`drift_retention`, `vibrato_retention`), but there is no
   separate manipulation surface for note center vs slow drift vs vibrato beyond these
   retention knobs. Not a defect; it is the current ceiling.

## Consequences for the research plan (proposal, not promotion)

- Keep `evidence_level: HYPOTHESIS_RESEARCH` for pitch-correction research hypotheses; this
  audit is source-mapping evidence (proof_strength 0.5: reproducible inspection, no audio
  measurement attached).
- Next executable research should drive the *product* chain (target engine + two-stage grain
  shifter) with the same synthetic contour corpus used in gate 4, so transformation evidence
  lands against the real baseline instead of a generic prototype.
- The V/UV threshold at `audio_input.cpp:12` is the highest-leverage product seam for a future
  candidate-based detector; changing it is a product decision (Cody), not a research inference.

## What this audit does NOT claim

No sound-quality, preference, usability, or real-time-headroom claim. No claim that the product
detector is good or bad on real vocals — only what its code surface is. REAPER/host evidence
remains required per AGENTS.md before any release-facing claim.
