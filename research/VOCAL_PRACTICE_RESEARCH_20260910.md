# Monkey's Ear — Vocal Practice Research

Date: 2026-09-10
Status: research guidance, not product authority
Scope: practical pitch/vocal processing improvements that can strengthen Monkey's Ear without forcing novelty.

## Direction

Monkey's Ear does not need every block to be unprecedented. The target is a good-sounding, fast, expressive, controllable instrument that uses Cody's ideas well. Conventional algorithms are welcome when they are the best tool for the job. Product identity should emerge from the combination of strong fundamentals with Monkey's Ear's own stateful, tuning, weighting and motion ideas.

Research therefore follows this rule:

> Prefer the best-known practical mechanism for a weak point, then integrate it with Monkey's Ear only where the integration improves sound, feel, control, robustness, or workflow.

Do not add novelty for novelty's sake.

## Current commercial baseline

### AutoTune 2026

Antares' current workflow still makes a small set of musician-facing controls do most of the work: Retune Speed, Humanize and Flex Tune, with Classic/Modern behavior and low-latency versus HQ operation. Antares describes zero Retune Speed as the hard Auto-Tune effect, Humanize as slower correction on sustained material, and Flex Tune as allowing expressive pitch variation near targets.

Practical lesson for Monkey's Ear:
- Keep a simple front layer that can produce a useful result with very few controls.
- Preserve separate concepts for correction speed, sustained-note behavior and expressive freedom rather than collapsing them into one intelligence control.
- Keep an intentionally hard/obvious correction sound available. Transparent behavior should not erase the usefulness of stylized tuning.

Source: Antares AutoTune 2026 FAQ and product documentation, accessed 2026-09-10.
- https://help.antarestech.com/hc/en-us/articles/42855736822932-AutoTune-2026-FAQ
- https://antarestech.com/products/pitch-correction/at2026

### Melodyne 5

Celemony's current vocal workflow explicitly separates pitch center, pitch modulation, pitch drift, transitions and formants. Its Melodic algorithm also treats sibilants/breath/noise-like material differently from pitched components during pitch and timing changes.

Practical lesson for Monkey's Ear:
- Keep center correction, slow drift, vibrato/modulation and note-to-note transition behavior independently controllable.
- Preserve consonant/sibilant/breath material when pitch transformation would damage it.
- Formant behavior should be independently controllable and transitions should matter, not just static formant shift.
- The existing periodic/mixed/aperiodic path is directionally correct, but should be judged by sound rather than promoted merely because it is structurally more elaborate.

Sources: Celemony Melodyne 5 documentation, accessed 2026-09-10.
- https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolPitch_2?env=standAlone
- https://helpcenter.celemony.com/M5/doc/melodyneAssistant5/en/M5tour_ToolModulationDrift_2
- https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolFormants?env=standAlone
- https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_NA_Mode_2?env=standAlone

## Recommended adoption map

### ADOPT / strengthen now

#### 1. Simple musical correction surface

Monkey's Ear can keep its deeper `VocalTargetEngine`, but the performer-facing layer should expose a small number of understandable controls first. Candidate high-level controls:
- TUNE — overall correction amount / center pull.
- SPEED — how quickly the corrected trajectory converges.
- HUMAN / FREE — how much local expressive deviation survives near a target.
- DRIFT — slow intonation stabilization independent of vibrato.
- VIBRATO — retain, reduce or exaggerate faster modulation.
- TRANSITION — connected-note glide/transition behavior.
- FORMANT — preserve or deliberately shift vocal identity.

These controls should map onto existing lower-level state rather than create a second processing authority.

#### 2. Better component protection

Continue improving the existing periodic/mixed/aperiodic path. The practical target is not perfect source separation. It is avoiding obvious damage to:
- sibilants,
- breaths,
- fricatives,
- plosive/attack energy,
- distorted/growled or weakly periodic material.

A low-confidence component should usually remain closer to the original signal unless a deliberate effect asks otherwise.

#### 3. Formant preservation as a first-class path

Pitch correction quality depends heavily on not dragging vocal resonances in an unnatural way. Strengthen formant estimation/preservation before adding more exotic vocal effects. Keep the option to exaggerate formant movement artistically.

Candidate architecture:
1. estimate broad spectral-envelope/formant state,
2. transform pitch-bearing periodic content,
3. restore or intentionally remap the envelope,
4. recombine protected aperiodic material,
5. smooth transitions causally.

This should remain compatible with LIVE/STUDIO/RENDER tiers: simple causal approximation in LIVE, richer estimation where latency/CPU permits.

#### 4. Transition quality before feature count

A tuner is often judged most harshly between stable notes. Focus testing on:
- scoops into target,
- falls away from target,
- legato interval changes,
- repeated detached notes,
- vibrato crossing scale boundaries,
- weak-pitch consonant-to-vowel transitions.

The existing articulation-aware target trajectory is a useful place to integrate this work.

### EXPERIMENT, because it fits Monkey's Ear

#### 5. Directional correction

Use the existing directional-gravity idea musically rather than as a novelty requirement. Upward and downward approaches may use different pull, release or glide behavior when that produces a better sound.

Useful possibilities:
- softer pull while approaching from below,
- firmer landing after overshoot,
- different release behavior on falls,
- preserving expressive upward scoops while stabilizing downward drift.

If a symmetric conventional path sounds better, keep the symmetric path.

#### 6. Tuning-space gravity

The arbitrary cents/ratio tuning engine is worth keeping because it can support both normal scales and personal/custom systems. Per-degree attraction, weak targets and directional gravity should be treated as optional expression tools layered over reliable normal tuning.

Normal 12-TET correction must remain easy and excellent.

#### 7. Stateful vocal response

ChronoState can influence vocal transformation where it creates useful touch response. Candidate low-risk mappings:
- phrase energy -> correction firmness,
- slow state -> formant/body movement,
- signed direction -> transition asymmetry,
- surprise/transient state -> temporary protection from correction.

Do not let ChronoState obscure predictable basic tuning. Zero modulation depth must behave conventionally.

#### 8. Harmonic/body shaping after pitch correction

Monkey's Ear already has independent weight/character and bounded harmonic motion. Rather than inventing a new pitch algorithm solely for uniqueness, use these post/parallel stages to give corrected vocals body, weight, aggression or movement.

This is a promising product distinction because a singer can get useful pitch correction and then immediately shape the corrected voice as an instrument.

### DEFER / avoid as priorities

Do not prioritize these merely because competitors have them:
- giant harmony stacks,
- generic vocoder modes,
- generic granular destruction,
- monster/creature presets,
- offline note-blob editing,
- AI branding,
- opaque one-knob "smart" correction.

They can be added later if they materially improve the product or fit a module, but none is required to validate Monkey's Ear.

## Artistic evaluation rule

Sound quality is ultimately judged by Cody's taste and musical use, with commercial products serving as references rather than authorities.

For each meaningful vocal change:
1. use the same source performance,
2. render Monkey's Ear current path,
3. render the candidate path,
4. compare against a relevant commercial reference when available,
5. match levels closely enough to avoid loudness bias,
6. listen for musical usefulness, artifacts, vocal identity, feel and character,
7. keep the version Cody prefers unless it violates hard engineering/reliability constraints.

Objective tests still own objective claims: build success, finite output, CPU/deadline behavior, allocations, locks, state recall, plugin latency and deterministic behavior. They do not own artistic preference.

## Next research/implementation targets

Highest-value order from the current architecture:

1. Repair deterministic/reproducible test behavior so A/B evidence is trustworthy.
2. Improve formant/envelope preservation through the existing PSOLA-primary path.
3. Improve consonant/breath/transient protection and recombination.
4. Refine transition behavior on real musical phrases.
5. Expose a small AutoTune-class front surface over the deeper target engine.
6. Use directional gravity/articulation as optional expressive extensions.
7. Explore ChronoState-to-vocal mappings only after normal correction sounds good.
8. Use SoundSpace/weight-character shaping to turn corrected vocals into a broader instrument.

## Promotion standard

A mechanism does not need to be unique. It earns promotion if it does one or more of the following:
- sounds better to Cody,
- feels better to perform through,
- gives useful control competitors make awkward,
- implements one of Monkey's Ear's existing ideas cleanly,
- improves reliability, latency or CPU behavior,
- makes a complex capability easier to use.

Novelty alone is not a promotion criterion.
