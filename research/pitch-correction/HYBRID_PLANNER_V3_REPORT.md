# Monkey's Ear Hybrid Planner V3 — Synthetic Evidence

Evidence state: SYNTHETIC / FAILURE-PRESERVING

## Scope
- 180 randomized vocal regions.
- 5 vowel-envelope families.
- F0: 95–330 Hz.
- Pitch shift: -9 to +12 semitones.
- Breath/noise: 0–0.28.
- Optional transients.
- Vibrato depth: 0–45 cents.
- Four candidate renders per region:
  - P_raw
  - P_cep
  - S_raw
  - S_cep

Total candidate renders evaluated: 720.

## Observable planner evidence
Candidate ranking features:
- source/candidate cepstral-envelope distance;
- resonance-peak displacement proxy;
- harmonicity loss;
- transient damage;
- high-frequency noise/aperiodicity proxy.

The planner was trained on 120 regions and held out on 60.

### Learned positive-weight planner
Weights:
- envelope distance: 8.4750
- resonance proxy: 0.4244
- harmonicity loss: 0.0717
- transient damage: 0.0652
- noise proxy: 0.4968

Held-out:
- mean oracle regret: 0.216
- exact oracle-best hit rate: 0.617

### Fixed-strategy held-out regret
- P_cep: 0.134
- P_raw: 0.581
- S_cep: 0.663
- S_raw: 1.437

Important failure: the trained linear planner did NOT beat the best fixed strategy (P_cep) on held-out oracle regret.

## Pareto frequency
Fraction of regions where a candidate remained on the observable Pareto front:
- P_cep: 0.956
- S_cep: 0.944
- S_raw: 0.872
- P_raw: 0.844

Interpretation: no renderer family is globally dominated. Candidate generation remains justified.

## Phrase continuity experiment
36 synthetic phrases × 8 regions = 288 phrase regions.
A dynamic-programming selector added source-relative seam damage and renderer-family switching cost.

Local baseline:
- oracle loss: -0.1595
- seam damage: 0.1115
- family switches: 2.6389

Dynamic selector, lambda=2.8:
- oracle loss: -0.1567
- seam damage: 0.1064
- family switches: 2.6111

This reduced seam damage by about 4.6% relative to local selection, but slightly worsened oracle quality. Lower lambdas barely changed seams. Therefore continuity needs its own representation and cannot be solved by a weak scalar penalty.

## Nonlinear planner experiment
Random-forest ranking on held-out regions:
- mean regret: 0.192
- median regret: 0.0
- oracle-best hit rate: 0.617

This slightly improved mean regret over the linear planner, but still failed to beat fixed P_cep.

A depth-5 deterministic decision tree distilled from the same evidence:
- oracle-best classification accuracy: 0.567
- mean regret: 0.258
- median regret: 0.0

The tree exposed useful nonlinear conditions involving:
- shift direction/magnitude,
- F0,
- vibrato depth,
- PSOLA harmonicity,
- candidate envelope damage.

But its held-out performance is not good enough to promote.

## Current conclusion
The architecture should NOT be:
`one scalar quality score -> one winning renderer`.

The evidence supports:
`candidate bank -> independent damage vector -> Pareto filtering -> task-specific policy -> continuity-constrained sequence decision`.

The strongest fixed synthetic path right now is PSOLA + cepstral envelope reconstruction, but phase/spectral candidates remain Pareto-relevant and win individual regions. The next experiment must use time-varying correction contours and explicit voiced/aperiodic splitting; otherwise the planner is optimizing region-level surrogates rather than actual vocal correction behavior.
