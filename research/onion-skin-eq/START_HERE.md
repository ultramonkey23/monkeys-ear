# Monkey's Ear — EQ research restart

## What is ready
Historical V1–V4 results and the corrected V3 source are archived here. V5 adds directional band-power and envelope-motion regression checks. Run `python research/onion-skin-eq/prototype_v3.py` and `python research/onion-skin-eq/directional_v5.py` with NumPy and pandas. These are research scripts, not product DSP.

## What the research actually establishes
V3 reproduces five synthetic heuristic classifications. V4 shows that opposing envelope motion can be measured independently of overlap. V5 verifies that a masker 12 dB stronger produces a +12 dB directional margin, reversing the gain produces -12 dB, and silent sources do not produce active interaction. None establishes perceptual masking accuracy. The -90 dBFS gate is an experimental engineering threshold, not an absolute hearing threshold.

## Design direction
Build an excellent conventional EQ first. Add optional temporal, track and state onion skins. Preserve raw spectral evidence separately from warnings, user priority and corrective action. Use directional target/masker relationships rather than a symmetric score alone. Harmonic relatedness and envelope complementarity are descriptors, not automatic masking exemptions. Preserve/Yield/Equal controls what the user permits, not what the analyzer claims to hear.

## Research to do next
Compare a conventional critical-band baseline with a documented auditory-filter excitation model. Account for level calibration, frequency spreading, time alignment and stereo. Use real stems and listening labels to evaluate false alarms and missed conflicts. Test silence, gain changes, equal/unequal tones, noise, harmonic/inharmonic material, sustained and transient sources. Do not fit thresholds to the five synthetic labels and call that validation.

## Product integration
First inspect current product master and existing EQ/analysis interfaces. Keep the main agent's MIDI-silence and Chrono-hum fixes as release blockers. Integrate through the existing Lab/Code Prime workflow, not a parallel rewrite. Preserve master history; reconcile the research-only commits rather than force-pushing over them. The current GitHub default branch is main, while the local product previously used master. Resolve that explicitly before convergence.

For live analysis, use bounded/preallocated work and timestamped data. VST3 processor/controller communication should use supported thread-safe mechanisms; do not send unbounded messages or perform file/network work on the audio callback. Keep warnings advisory and corrective EQ opt-in, bounded, automatable and reversible. Do not promise future audio lookahead without explicit latency.

## Sources
- Auditory excitation and masking: https://pmc.ncbi.nlm.nih.gov/articles/PMC2606789/
- Cambridge loudness models: https://pmc.ncbi.nlm.nih.gov/articles/PMC4227665/
- Level-dependent critical bands: https://pubmed.ncbi.nlm.nih.gov/711992/
- Multicomponent masking: https://pubmed.ncbi.nlm.nih.gov/3403798/
- Programme loudness (not a masking model): https://www.itu.int/rec/R-REC-BS.1770-5-202311-I
- VST3 communication: https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Communication.html
- VST3 data exchange: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Data%2BExchange/Index.html

## Evidence status
Synthetic tests: completed. Real-stem evaluation: pending. Listening validation: pending. Product integration: pending. No automatic corrective EQ approved.
