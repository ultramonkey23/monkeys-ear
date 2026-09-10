# Candidate renderer planner v1

Status: **SYNTHETIC / FAILURE-PRESERVING**. This is research evidence only, not production DSP or listening proof.

## Question

Can Monkey's Ear generate multiple vocal pitch-shift candidates for the same short region, reconstruct formants independently, and choose the least-damaging candidate from source-referenced measurements rather than hard-wiring one renderer?

## Experiment

`experiments/candidate_planner_v1.py` generates eight deterministic source/filter vocal regions spanning clean small/mid/large shifts, breathy material, transient-bearing material, and a high-F0 case. For each region it creates six candidates:

- PSOLA raw
- PSOLA + CEP40 envelope reconstruction
- PSOLA + LPC20 envelope reconstruction
- identity-phase-locking-style spectral raw
- spectral + CEP40 envelope reconstruction
- spectral + LPC20 envelope reconstruction

The planner is intentionally not allowed to see the synthetic oracle. It scores candidates only by source-referenced cepstral-envelope distance plus a harmonicity penalty. The oracle is used afterward only to measure regret against the known unchanged formant geometry.

## Result

The first planner **failed in an informative way**: it selected `SPECTRAL_cep40` in all eight regions. That candidate had strong broad-envelope reconstruction, so the planner score collapsed onto it instead of truly distinguishing renderer damage.

Across the eight selected regions, `SPECTRAL_cep40` averaged approximately **1.12 dB oracle envelope RMSE**, but about **126.8 Hz mean formant-peak error**. By comparison, the PSOLA family preserved formant peak locations much better in this corpus (roughly **58.6 Hz mean** for the CEP40 PSOLA variant) even though its broad-envelope RMSE was worse.

The selected candidate matched the experiment's composite oracle-best candidate in only **4 of 8 cases (50%)**. Important misses included the clean mid-shift, breathy large-shift, transient large-shift and clean small-shift cases.

## Interpretation

This rejects a naive version of the renderer-planner idea. A planner cannot safely use the same reconstructed-envelope similarity that one candidate family is explicitly optimized to minimize. That makes the selector self-referential and lets envelope reconstruction hide other damage.

The useful architecture survives, but the scoring layer must become genuinely multi-objective and partially independent from the renderers. Candidate selection should retain separate evidence for at least:

- spectral-envelope preservation;
- formant/resonance displacement;
- periodicity/harmonic continuity;
- transient/onset damage;
- aperiodic/noise preservation;
- boundary discontinuity when switching renderers;
- transformation magnitude and detector confidence.

If the evidence disagrees or confidence is low, the planner should keep multiple candidates/Pareto choices rather than forcing a single winner. For LIVE mode this likely becomes a bounded causal policy; STUDIO/RENDER can afford heavier analysis and local A/B candidate evaluation.

## Next experiment

1. Replace the self-referential scalar score with a Pareto/multi-objective selector.
2. Add an independent resonance/formant tracker rather than deriving all selection evidence from CEP40.
3. Split periodic and aperiodic energy so breath/sibilance is not judged by harmonic metrics alone.
4. Add transition-boundary cost so per-region renderer switches cannot create clicks or timbral jumps.
5. Drive the candidate renderer from the existing center/drift/modulation/transition target contour rather than only fixed region shifts.
6. Preserve this failed scalar planner as a regression case: a future selector must not collapse to one candidate merely because that candidate dominates the metric used to score it.
