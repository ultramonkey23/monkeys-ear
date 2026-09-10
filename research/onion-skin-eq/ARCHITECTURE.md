# Monkey's Ear EQ — architecture source of truth

Status: active design guidance for the **EQ / spectral-dynamics module only**. This is not the architecture of the full Monkey's Ear system. Synthetic evidence exists; real-stem listening validation and product DSP integration are still pending.

## Product goal

Build an excellent normal EQ first, then add deeper mechanisms only when they improve a defined musical task without unacceptable collateral damage. The normal surface stays familiar: filter type, frequency, gain and Q. Dynamics, source relationships, harmonic-family behavior and other advanced controls appear only when the user deliberately uses them.

Automatic behavior is a convenience layer, not the research objective and not the architecture driver.

## Core architectural split

Do not build one opaque smart-EQ law. Keep these responsibilities independent:

- **Filter engine** — conventional EQ quality, phase behavior, modulation safety and automation.
- **Evidence** — spectral energy, transient/body behavior, harmonic grouping, pitch confidence, auditory-filter/excitation neighborhoods, temporal history and source relationships.
- **Intent** — explicit Preserve / Yield / Equal direction and user-defined strength/bounds.
- **Candidate generators** — conventional band gain, dynamic band gain, harmonic-family gain and future experimental actions.
- **Projection gate** — evaluate a proposed bounded action before commitment; reject actions that fail the current objective, violate protected structure or exceed stability/identity limits.
- **Action** — only accepted, bounded, reversible changes reach DSP.
- **History** — accepted/rejected action history, persistence, return and resistance only where evidence shows they beat simpler timing laws.

This separation lets better perceptual models replace weaker ones without rewriting the entire EQ.

## Current priority order

1. Conventional filter quality and phase behavior.
2. Dynamic EQ / compression math, including transient versus sustained/body behavior.
3. Harmonic-family analysis as a candidate generator.
4. Projected bounded action as a reusable safety/control primitive.
5. Directional cross-texture relationships and Preserve / Yield / Equal intent.
6. Onion-skin temporal evidence and history.
7. Optional persistence, resistance and return when independently justified.
8. Phase, spatial and harmonic-motion experiments only when they beat simpler methods for a defined task.
9. Automatic behavior last, as a bounded UX wrapper over proven mechanisms.

## Active decisions

### Promote
- Excellent conventional EQ and conventional dynamic EQ as the required baseline.
- Harmonic-family **analysis** and auditory-weighted targeting as candidate generation, not unquestioned action laws.
- Directional user intent.
- Transient/body-aware evidence.
- Projected bounded action.
- Inspectable, automatable, reversible and recallable actions.

### Keep as explicit tools / baselines
- Broad EQ carving. It may intentionally change timbre and is often musically correct when the user asks for stronger separation.
- Conventional multiband/dynamic EQ behavior. New mechanisms must beat it for a defined task, not merely be more novel.

### Experimental / must re-earn complexity
- Full relational state dynamics. Persistence, resistance, return and transient protection are individually testable; there is not yet enough evidence for one monolithic state engine.
- Phase/spatial intervention.
- Harmonic pitch movement or detuning.

### Retired as defaults or design objectives
- Treating overlap itself as an error.
- Maximizing spectral separation as the sole score.
- Fixed harmonic attenuation merely because a conflict detector fired.
- Unconstrained harmonic detuning.
- Automatic phase/stereo movement.
- One giant opaque smart-EQ law.
- Synthetic thresholds treated as perceptual truth.
- Hidden future/lookahead claims without explicit latency.

## Current-practice benchmark

Monkey's Ear must not mistake established ideas for differentiation. Transient/tonal splitting, resonance suppression, spectral compression, semantic compressor controls, dynamic EQ and masking-style spectrum views already exist in commercial tools. The differentiator worth pursuing is **relational, directional and inspectable control**: which source is preserved, which yields, what evidence supports that decision, and whether a candidate correction actually helps before it is committed.

References retained for benchmarking:
- Eventide SplitEQ: https://www.eventideaudio.com/plug-ins/spliteq/
- oeksound soothe2: https://oeksound.com/manuals/soothe2/
- sonible smart:comp lineage: https://www.sonible.com/blog/smartcomp3-out-now/

## Psychoacoustic guardrails

- Harmonicity, F0 relationships, peripheral frequency resolution, onset relationships and phase can all affect grouping or masking.
- Lower resolved harmonics are important for pitch and source segregation; do not casually modify them.
- ERB/excitation models are useful engineering baselines but are not complete hearing or masking models.
- Overlap can be desirable. Intended blends and shared harmonics must not be punished automatically.

Key references:
- Moore & Glasberg auditory-filter work: https://pubmed.ncbi.nlm.nih.gov/6630731/
- Level-dependent excitation/frequency selectivity: https://pubmed.ncbi.nlm.nih.gov/3654390/
- Limits of excitation-pattern masking prediction: https://pubmed.ncbi.nlm.nih.gov/7852202/
- Harmonicity and concurrent-source segregation review: https://pmc.ncbi.nlm.nih.gov/articles/PMC2885481/

## Real-time integration rules

Before product integration, inspect the current Monkey's Ear source and existing EQ/dynamics interfaces. Do not create a parallel rewrite.

Live analysis/control must use bounded and preallocated work, timestamped data and supported processor/controller communication. No file I/O, network I/O, blocking, unbounded allocation, model inference or training in the audio callback. Every corrective action must remain automatable, reversible, recallable and bypassable.

## Evidence and promotion rules

Engineering proxies are not perceptual claims. Collision, ERB overlap, identity and energy metrics can falsify bad ideas, but they do not prove clarity, depth, punch or preference.

A mechanism stays active only if it does at least one of these:

1. beats a simpler baseline on a defined engineering tradeoff without unacceptable damage;
2. wins a level-matched controlled listening task for a defined musical purpose;
3. provides a clearly different user-controlled creative capability.

Otherwise simplify it, demote it, archive the evidence or remove it.

## Next research gate

Build a more faithful documented auditory-filter/excitation reference, make the objective directional target-versus-masker rather than symmetric overlap, add transient/body and onset-synchrony cases, compare **projected conventional dynamic EQ** against **projected harmonic-family control**, then move to level-matched real stems and blinded listening. Measure clarity, depth, punch, timbral damage, preference, CPU, latency, zippering, worst-case control movement, mono behavior and phase effects.