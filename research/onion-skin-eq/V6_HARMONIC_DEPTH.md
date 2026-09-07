# V6 — Dynamic harmonic depth

Status: exploratory engineering research. Not perceptual validation and not production DSP.

## Question
Can Monkey's Ear preserve depth and clarity between overlapping textures by treating harmonic families as controllable dynamic relationships rather than relying only on broad EQ carving or full-band compression?

## Result
Synthetic matched harmonic complexes were compared under changing Preserve/Equal intent. A bounded control-rate model protected the first two harmonics, applied at most 2.5 dB of upper-partial yielding, and optionally added tiny state-smoothed upper-partial movement.

- Untouched mean collision proxy: 0.3342
- Dynamic harmonic gain: 0.3293
- Dynamic harmonic gain + bounded movement: 0.3291
- Mean harmonic-envelope identity remained 0.9994 or better.
- Actual maximum movement reached only 2.1 cents in this test because movement was scaled by partial conflict.

The collision number is an engineering proxy, not a perceptual masking score. It uses a deliberately simple 25-cent spectral-neighborhood weighting and must not be promoted to an auditory model.

## Decision
**Promote:** harmonic-family gain negotiation controlled by user intent, bounded state dynamics, and explicit protection of pitch-defining/fundamental content.

**Keep experimental:** tiny upper-partial movement. It contributed very little additional improvement in V6 and can introduce beating, timbral instability, or unintended source segregation. It must earn its complexity in listening tests before product integration.

**Retire as a default idea:** unconstrained or broad harmonic detuning as a clarity mechanism. Psychoacoustic literature shows harmonicity is a strong auditory grouping cue and that sufficient mistuning can cause a harmonic to perceptually separate or 'pop out'. Movement belongs in a tightly bounded optional/creative lane, not as the normal corrective action.

## Design evolution
The active concept is now a Dynamic Spectral Relationship Engine rather than a masking-warning EQ. Conventional EQ remains the base. Above it sit independent evidence and action layers:

1. spectral/temporal evidence;
2. transient vs sustained/body evidence;
3. harmonic-family and pitch-confidence evidence;
4. user intent: Preserve / Yield / Equal;
5. bounded corrective actions: dynamic gain first, then optional spatial/phase/harmonic movement;
6. state smoothing / resistance / return to prevent chatter.

Compression and EQ should share this control layer. A region can respond differently to transients, sustained energy, repeated pressure, or competing sources without forcing the whole signal through one detector/ratio law.

## Psychoacoustic guardrails
- Harmonic components sharing an F0 tend to group perceptually; F0 differences can support segregation.
- Lower resolved harmonics are particularly important for pitch and concurrent-source segregation.
- Mistuned components can become perceptually distinct; literature reports effects around a few percent, so tiny-cent movement must not be confused with a guaranteed safe threshold.
- Onset synchrony and common modulation also contribute to grouping. A future model therefore cannot use spectral overlap alone.
- Phase of harmonic complexes can materially alter masking even when long-term power spectra are identical, so phase cannot be dismissed as purely cosmetic.

## Next experiments
1. Replace the 25-cent collision proxy with a documented auditory-filter / excitation-pattern baseline.
2. Add transient/body decomposition and compare dynamic harmonic gain against broad EQ and conventional multiband compression.
3. Test same-F0 vs different-F0 textures, resolved vs unresolved harmonics, common vs asynchronous onsets, and harmonic vs inharmonic material.
4. Add phase-sensitive cases without allowing phase automation to destabilize pitch or mono compatibility.
5. Render level-matched real stems and run blinded listening labels for clarity, depth, punch, timbral damage, and preference.
6. Measure CPU, latency, state stability, zippering, and worst-case control movement before product integration.

## Sources
- Micheyl & Oxenham review on pitch, harmonicity and concurrent sound segregation: https://pmc.ncbi.nlm.nih.gov/articles/PMC2885481/
- Harmonicity aids auditory grouping/hearing in noise: https://pmc.ncbi.nlm.nih.gov/articles/PMC8803411/
- Spectral processing and source determination; small F0 differences can aid segregation: https://pmc.ncbi.nlm.nih.gov/articles/PMC2535849/
- Mistuned harmonic / inharmonicity scene-analysis evidence: https://pmc.ncbi.nlm.nih.gov/articles/PMC3641774/
- Harmonic-complex phase can change masking despite equal long-term spectra: https://pmc.ncbi.nlm.nih.gov/articles/PMC3201066/
- Eventide SplitEQ as current-practice evidence for separating transient and tonal streams: https://www.eventideaudio.com/plug-ins/spliteq/

## Integration rule
No automatic product integration from this research. Inspect current Monkey's Ear source and existing EQ/dynamics interfaces first. Keep MIDI silence and Chrono hum as release blockers. Production actions must be bounded, automatable, reversible and audio-thread safe.
