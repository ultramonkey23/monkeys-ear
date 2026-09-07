# V9 — Auditory-weighted projected harmonic control

Status: synthetic engineering research for the **Monkey's Ear EQ module only**. Not perceptual validation, not production DSP, and not proof of audible improvement.

## Why V9 exists

V8 used a simple spectral collision proxy. That proxy over-rewarded broad carving and could not represent auditory frequency selectivity. V9 replaces it with an ERB/excitation-neighborhood engineering baseline and then deliberately tries to break the promoted harmonic-family idea across perturbed source spectra.

Classic auditory-filter work provides ERB and excitation-pattern foundations, but excitation-pattern models are not complete masking models and can fail on some masking conditions. V9 therefore treats the auditory model as a better engineering baseline, not as ground truth for hearing.

Primary references:
- Moore & Glasberg (1983), auditory-filter bandwidths and excitation patterns: https://pubmed.ncbi.nlm.nih.gov/6630731/
- Moore & Glasberg (1987), level-dependent frequency selectivity and excitation patterns: https://pubmed.ncbi.nlm.nih.gov/3654390/
- van der Heijden & Kohlrausch (1994), limits of excitation-pattern masking prediction: https://pubmed.ncbi.nlm.nih.gov/7852202/
- Micheyl & Oxenham review on harmonicity and concurrent-source segregation: https://pmc.ncbi.nlm.nih.gov/articles/PMC2885481/

## V9A — tougher five-way comparison

Five strategies were tested over 4,000 perturbed harmonic source pairs:

1. broad carve;
2. conventional dynamic EQ;
3. V8 harmonic-family control;
4. auditory-weighted harmonic control;
5. auditory-weighted harmonic control plus mild contiguous support.

Important result: **all five fixed-form strategies had negative lower-decile cases** under the ERB-overlap proxy. In other words, a rule that usually helps can still make the current pair worse.

That invalidates any architecture that blindly applies a learned or hand-authored harmonic attenuation law merely because a conflict detector fired.

## V9B — projected control

The stronger mechanism is a projection/acceptance layer:

1. generate a small bounded candidate action;
2. preserve protected low/pitch-defining harmonics;
3. evaluate the candidate against the current objective;
4. reject it if it does not improve the objective;
5. reject it if identity bounds are violated;
6. repeat only while useful bounded improvements remain.

In the current prototype the first four harmonics are protected and candidate attenuation occurs in 0.25 dB steps with an upper action bound determined by user/severity state.

### 2,000-case perturbed-spectrum result

- mean ERB-overlap improvement: **0.5164%**
- 10th-percentile improvement: **0.0%**
- 1st-percentile improvement: **0.0%**
- cases with zero-or-better objective change: **100%**
- mean accepted steps: **4.259**
- 1st-percentile yielded-source spectral identity: **0.998751**
- minimum protected-low-harmonic identity: **1.0** in this implementation
- mean combined energy change: **-0.004861 dB**
- 5th-percentile combined energy change: **-0.027938 dB**

The ERB-overlap improvement is intentionally modest. The important result is that the controller can decline to act. **No correction is better than a mathematically unjustified correction.**

## Decision

**PROMOTE — projected bounded action as an architecture primitive.** Proposed corrective actions should be evaluated before commitment whenever the analysis representation allows it. The projection layer is reusable across dynamic gain, harmonic-family actions, phase experiments, and later relationship control.

**PROMOTE WITH QUALIFICATION — auditory-weighted harmonic-family targeting.** It is now a candidate generator, not an unquestioned action law.

**DEMOTE — V8 fixed harmonic-family attenuation law.** Keep it as historical evidence/baseline. Do not implement it directly as product behavior.

**KEEP — conventional dynamic EQ and broad carving as required baselines.** They remain essential comparisons and explicit musical tools.

**REJECT — one-score optimization.** ERB overlap, spectral collision, masking prediction, clarity, and user preference are not interchangeable. Production decisions need multiple objectives plus listening evidence.

## Architectural consequence

The durable core should separate:

- **evidence:** spectrum, transient/body, harmonic grouping, ERB/excitation neighborhood, source relationship;
- **intent:** Preserve / Yield / Equal and explicit user setup;
- **candidate generator:** conventional band, harmonic family, relationship, phase/spatial experiment;
- **projection gate:** bounds, protected structure, predicted benefit, stability constraints;
- **action:** only the accepted bounded change;
- **history:** previous accepted/rejected actions, persistence and return where justified.

This avoids one giant smart-EQ law and lets better perceptual models replace weaker ones later without replacing the entire dynamics architecture.

## What is still missing

V9 does **not** include calibrated SPL, a canonical ROEX implementation, cochlear compression, temporal fine structure, binaural effects, forward masking, real stems, listener labels, or production DSP timing/CPU measurements. The excitation approximation is deliberately lightweight.

## Next gate

1. implement a documented auditory-filter/excitation reference more faithfully;
2. add directional target-vs-masker excitation rather than only symmetric overlap;
3. add transient/body and onset-synchrony cases;
4. test projected conventional dynamic EQ versus projected harmonic-family control;
5. test real, level-matched stems;
6. run blinded listening for clarity, depth, punch, timbral damage and preference;
7. promote only mechanisms that beat a simpler baseline for a defined task.

## Durability rule

A previous promoted idea can be demoted when a tougher test exposes a weakness. Historical evidence stays reproducible, but active guidance must point to the strongest surviving mechanism rather than preserving old conclusions as doctrine.
