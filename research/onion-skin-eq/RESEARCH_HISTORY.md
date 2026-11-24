# Monkey's Ear EQ — research history and evidence ledger

Purpose: preserve provenance without letting old research compete with current design guidance. **ARCHITECTURE.md is the source of truth.** This file records what each research generation established, what survived, and what was demoted.

## Evidence status

Synthetic engineering tests: active. ERB/excitation model: active but approximate. Real-stem evaluation: pending. Blinded listening: pending. Perceptual masking validation: pending. Product integration: pending.

## V1–V5 — historical interaction heuristics

V1–V3 explored spectral interaction/overlap heuristics. V4 separated envelope complementarity from overlap. V5 verified directional band-power behavior and silence/inactive states.

Surviving lesson: interaction evidence must distinguish coexistence, pressure and direction rather than treating overlap as inherently bad.

Status: historical baseline only. Synthetic labels and thresholds are not perceptual ground truth.

Reproducible artifacts retained in this directory include `prototype_v3.py`, `directional_v5.py`, `directional_v5.csv`, `envelope_v4.csv` and early interaction CSVs.

## V6 — harmonic depth

Question: can overlapping textures be handled through harmonic-family relationships instead of only broad carving or full-band compression?

Synthetic result: bounded upper-partial gain negotiation reduced the simple collision proxy while preserving harmonic-envelope identity around 0.9994+. Tiny bounded partial movement added almost no extra benefit in that test.

Surviving lesson: harmonic-family analysis is worth pursuing, but pitch-defining/lower harmonics require protection.

Demoted: harmonic detuning as a default clarity mechanism. It remains at most an explicit creative experiment because mistuning can change perceptual grouping and timbre.

Reproducible artifacts: `v6_dynamic_harmonic_depth.py`, `v6_dynamic_harmonic_depth.csv`.

## V7 — automatic-dynamics guardrail

Question: can a default automatic lane stay bounded and low-chatter while user-explicit dynamics remain stronger?

Synthetic controller experiments showed this is feasible: transient-aware conservative control could remain far below an intentionally naive 4 dB detector follower, while an explicit user lane could permit materially larger movement.

Surviving lesson: automatic behavior can be a quiet UX layer, but it is **not a core research priority** and must not drive the EQ architecture.

Reproducible artifacts: `v7_auto_dynamics.py`, `v7_auto_dynamics_metrics.csv`.

## V8 — core strategy comparison

Compared broad static carving, conventional dynamic EQ, harmonic-family control and relational state dynamics.

Deterministic synthetic result:

| Strategy | Collision improvement | Identity floor | Mean energy delta | Control-motion index |
|---|---:|---:|---:|---:|
| Broad static carve | 4.58% | 0.9970 | -0.160 dB | 0.750 |
| Conventional dynamic EQ | 2.72% | 0.9985 | -0.112 dB | 0.649 |
| Harmonic-family control | 1.14% | 0.9998 | -0.060 dB | 0.154 |
| Relational state dynamics | 0.90% | 0.9998 | -0.061 dB | 0.167 |

A 2,000-state randomized sweep reproduced the broad tradeoff: carving wins a simplistic separation metric but changes more of the sources; harmonic-family control is more conservative and preserves identity.

Surviving lesson: one-score optimization is wrong. Maximize-separation was retired as an objective.

Demoted: the fixed V8 harmonic attenuation law. V9 showed fixed rules can worsen harder auditory-weighted cases.

Reproducible artifacts: `v8_core_comparison.py`, `v8_core_comparison_scored.csv`, `v8_monte_carlo_summary.csv`.

## V9 — auditory weighting and projected control

V9 replaced raw spectral-collision emphasis with an approximate ERB/excitation-neighborhood engineering baseline and intentionally tried to break the promoted V8 idea across perturbed source spectra.

### V9A

A 4,000-case five-strategy sweep showed negative lower-decile cases for every fixed-form strategy tested. A conflict detector therefore cannot safely authorize a fixed corrective law by itself.

### V9B

Projected control was introduced:

1. generate a small bounded candidate action;
2. protect important structure;
3. evaluate the candidate against the current objective;
4. reject it if it does not help or violates bounds;
5. repeat only while useful bounded improvements remain.

Across 2,000 perturbed source pairs, the current prototype achieved:

- mean ERB-overlap improvement: 0.5164%;
- 10th- and 1st-percentile improvement: 0.0%;
- zero-or-better objective movement: 100% of cases;
- mean accepted steps: 4.259;
- 1st-percentile yielded-source identity: 0.998751;
- protected first-four-harmonic identity: 1.0 in this implementation;
- mean combined energy change: -0.004861 dB.

These are engineering metrics, not listening proof.

Surviving lesson: **projected bounded action is the strongest current architecture primitive.** Harmonic/auditory analysis proposes actions; it does not get automatic authority to commit them.

Reproducible artifacts: `v9_projected_auditory.py`, `v9_projected_summary.csv`.

## Current benchmark against existing practice

Transient/tonal EQ, resonance suppression, spectral compression, semantic compressor controls and dynamic EQ already exist commercially. Those ideas remain useful but are not sufficient differentiation on their own. The research moat is relational and directional control with explicit evidence, user intent, candidate generation and bounded acceptance.

## Change policy

Old research should not spawn a permanent design document. New generations should update **ARCHITECTURE.md** and append a concise result here. Keep executable experiments/data when they remain useful for reproduction; remove duplicated narrative documents once their durable conclusions are captured here.

A tougher experiment may demote an earlier conclusion. That is expected behavior, not research failure.

## 2026-11-24 — Production bounded cascade projection

The native `OnionSkinEQ` (shared by the instrument and standalone Onion EQ)
now separates its existing occupancy evidence and intent from accepted dynamic
action. `onion_projection::select` evaluates bounded alternatives against the
same before/after conflict proxy, identity floor and physical peaking-cascade
response sampled at the analyzer centers. A candidate must improve the proxy;
otherwise it returns neutral. The strongest unscaled proposal defines the
combined dynamic response budget, rather than allowing neighboring filters to
multiply that allowance. This is a new tested engineering choice, not a
creator-approved perceptual law. Manual EQ remains separate and unconstrained
by the dynamic projection budget. Harmonic protection first proposes localized
reductions in filters that influence protected centers, then re-evaluates; a
bounded global backoff is the fallback.

Native GCC C++20 -O3 -ffast-math proof used source/header blobs matched to
commits `4e0c161` and `e1fc57a`: 4,000 independent occupancy-proxy cases
(3,428 accepted; zero bound violations); 2,700 cascade cases across 32, 48,
96 kHz and Q 0.85/1.85/4.5 (all accepted; independent complex transfer-function
bounds and conflict/identity checks passed); actual 878 Hz audio processing
and manual-EQ independence passed. At 48 kHz and Q 1.85, the old and
first-stage implementation produced wet/dry energy 0.0077581 (about -21.1 dB)
under the maximum-authority 878 Hz fixture; the cascade-aware implementation
produced 0.249007 (about -6.0 dB). The two-tone fixture at Q 1.85 with harmonic
protection off/on measured 255 Hz -5.008/-0.905 dB and 4275 Hz
-5.969/-5.204 dB. Local proof is reproducible through
`monkeys_ear_onion_projection_probe` and the added audio-consumer regression
in `monkeys_ear_test_runner`. The Windows runner now requires the new probe.

Evidence scope: analytic and synthetic native DSP execution, not a full
Windows VST3/REAPER build, physical-device timing, real-stem listening,
or perceptual masking validation. The projection is of a steady dynamic-only
response at discrete centers; intermediate smoothing, inter-center extrema,
manual-filter interactions, and the full V9 excitation/harmonic-family
candidate research remain open. The current 0.86 identity threshold and
finite search/backoff schedule are engineering priors to compare against
musician goals and real evidence. An isolated broadband 480,000-sample
benchmark measured 132–140 ms for the new core versus 107–121 ms for the
first-stage core; this adds work and does not establish whole-engine
real-time margin. No subjective sound-quality promotion is implied.
