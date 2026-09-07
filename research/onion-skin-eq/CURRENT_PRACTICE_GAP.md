# Current-practice gap — what Monkey's Ear must beat

Status: design benchmark, September 2026. This document exists to prevent novelty drift and feature accumulation.

## Current practice already covers more than normal EQ/compression

### Eventide SplitEQ
Separates incoming audio into transient and tonal streams and lets the user EQ and spatially manipulate them independently. Therefore 'separate attack from body before EQ' is useful but not by itself a Monkey's Ear differentiator.
Source: https://www.eventideaudio.com/plug-ins/spliteq/

### oeksound soothe2
Analyzes resonant behavior and applies dynamic, frequency-selective reduction with user-shaped sensitivity. Therefore 'automatically reduce harsh resonances only when active' is established practice, not a unique thesis.
Source: https://oeksound.com/manuals/soothe2/

### sonible smart:comp 3 / spectral compression lineage
Current sonible practice includes signal-adapted compression behavior, semantic/perceptual navigation of compression character, traditional parameter synchronization, and spectral compression in the product lineage. Therefore 'let the user say punchy/dense/dynamic instead of only threshold/ratio' is not enough.
Sources:
- https://help.sonible.com/hc/en-us/articles/24338576473500-What-is-the-Compression-Matrix-and-what-does-the-Compression-Scope-show
- https://help.sonible.com/hc/en-us/articles/24338585620380-What-is-the-main-difference-between-smart-comp-2-and-smart-comp-3
- https://www.sonible.com/blog/smartcomp3-out-now/

## Monkey's Ear differentiator must be relational
The active product gap is not another smarter single-track processor. The stronger direction is an inspectable relationship engine that can answer:

- Which source/texture is being preserved, which may yield, and under what conditions?
- Which transient, body, harmonic family, stereo/phase component, or time skin owns the collision?
- Can the correction act on the competing relationship instead of broadly changing either source?
- Can the user see the evidence separately from the proposed action?
- Can the system return smoothly to neutral rather than chatter around a detector threshold?

## Active mechanism hierarchy
1. Excellent conventional EQ and compressor behavior.
2. Transient/body decomposition where useful.
3. Directional interaction evidence between sources.
4. Harmonic-family evidence with pitch-confidence / unknown states.
5. Preserve / Yield / Equal contracts.
6. Bounded dynamic gain/compression as first corrective action.
7. State smoothing, resistance and return.
8. Optional spatial/phase intervention if mono/transient constraints remain safe.
9. Tiny harmonic movement only as an experimental/creative action until listening proves value.

## Explicitly not unique / not enough
- Dynamic EQ alone.
- Spectral compression alone.
- Resonance suppression alone.
- Transient-vs-tonal EQ alone.
- Semantic compressor macro controls alone.
- Cross-track spectrum display alone.
- Automatic masking warnings alone.

## Research moat to pursue
Combine source relationships with harmonic grouping and time history. Psychoacoustic work indicates harmonicity, F0 separation, onset relationships, peripheral resolution and phase all contribute to auditory grouping or masking. Monkey's Ear can use these as evidence dimensions while leaving creative priority to the user.

Key background:
- https://pmc.ncbi.nlm.nih.gov/articles/PMC2885481/
- https://pmc.ncbi.nlm.nih.gov/articles/PMC8803411/
- https://pmc.ncbi.nlm.nih.gov/articles/PMC2535849/
- https://pmc.ncbi.nlm.nih.gov/articles/PMC3201066/

## Kill rule
A new feature is not promoted because it sounds novel in conversation. Promote it only if it either:
(a) improves a defined measurable/control objective without unacceptable damage, or
(b) wins a level-matched listening task for a defined musical purpose.
Otherwise revise, archive, or remove it.
