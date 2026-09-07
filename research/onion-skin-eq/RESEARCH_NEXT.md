# Research handoff — Onion Skin EQ

Status: exploratory. No real-audio masking validation, product integration, or listening acceptance. The historical V3 labels are heuristic outputs, not ground truth.

## Corrections
The original ZIP notebook accidentally captured the packaging script rather than the V3 experiment. The corrected source is prototype_v3.py, which reproduces the archived V3 table. Preserve the original ZIP only as historical provenance; do not use its notebook as the experiment implementation. V1/V2 numerical tables are archived, but their complete source was not recovered. The V4 envelope table is preserved; its exact historical source was not recovered. Do not invent missing provenance.

## Research findings
Auditory-filter excitation patterns are a stronger foundation than arbitrary FFT-bin overlap. Masking is level-dependent and can involve suppression, off-frequency listening, and nonlinear interactions. Harmonic relatedness does not imply absence of masking. Relative occupancy alone cannot establish absolute audibility. A stable or yielding envelope does not prove that a source is unmasked.

Sources:
- https://pmc.ncbi.nlm.nih.gov/articles/PMC2606789/
- https://pmc.ncbi.nlm.nih.gov/articles/PMC4227665/
- https://pubmed.ncbi.nlm.nih.gov/711992/
- https://pubmed.ncbi.nlm.nih.gov/3403798/
- https://www.itu.int/rec/R-REC-BS.1770-5-202311-I
- https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Communication.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Data%2BExchange/Index.html

## Next experiment
Build a causal analysis-only model with absolute dBFS band power, explicit gain calibration, separate overlap and directional masking-risk estimates, and confidence/unknown states. Compare a conventional critical-band baseline against an auditory-filter excitation baseline. Preserve raw measurements, source gain, time alignment, and independent user intent. Do not tune thresholds against the five synthetic labels as though they were listening truth.

Test silence, gain changes, equal-level and unequal-level tones, nearby and distant frequencies, noise, harmonic and inharmonic sources, stable/yielding/following envelopes, repeated transients, sustained sources, and stereo separation. Use real stems and blinded listening before claiming perceptual accuracy. Measure false positives, false negatives, latency, and CPU cost. No automatic EQ until the evidence is credible.

## Integration
Inspect current product master and existing EQ/analysis interfaces before changing code. Use the Lab's existing Code Prime ownership and master-convergence workflow. Keep research isolated until accepted. No audio-thread network, disk, blocking locks, or unbounded work. Analysis timestamps must account for DAW routing and plugin latency. VST3 processor/controller communication must follow supported thread-safe mechanisms. Keep MIDI silence and Chrono hum as release blockers.
