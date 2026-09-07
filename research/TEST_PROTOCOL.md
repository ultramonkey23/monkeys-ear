# Monkey's Ear — shared audio evaluation protocol

Status: durable test guidance. This protocol is meant to let one recorded source become reusable evidence across multiple Monkey's Ear subsystems.

## Why this exists

Every processor should not be tested on a different convenient clip. A small controlled corpus lets pitch correction, EQ, compression, saturation, modulation, spatial processing and later synthesis/capture work be compared against one another and against external tools without changing the source underneath the experiment.

## Core corpus rule

Keep the original dry recording immutable. Derive all test renders from that exact source and store enough metadata to reproduce them.

Minimum metadata per source:
- source ID and human-readable name;
- file hash;
- sample rate / bit depth / channel layout;
- recording chain when known;
- trim boundaries and gain normalization, if any;
- permission/provenance;
- musical context: key/scale, tempo, intended notes/phrasing when known.

Do not overwrite the dry master with corrected/processed audio.

## Initial vocal comparison matrix

Run the same recorded vocal through:

1. dry/unprocessed reference;
2. Monkey's Ear pitch correction;
3. Melda MAutoPitch;
4. every other pitch-correction tool available to Cody;
5. optionally a manual/offline reference correction when practical.

For each system save at least:
- natural/subtle setting;
- medium correction;
- strong/obvious correction if the tool supports it;
- exact settings or preset state;
- rendered WAV;
- measured latency where relevant.

Do not force parameter names to match between products. Match the **musical task**, not the UI label.

## Pitch-correction evaluation dimensions

Engineering measurements should include, where the available reference allows:
- voiced/unvoiced classification errors;
- F0 tracking continuity and octave errors;
- note-center error;
- transition timing error;
- vibrato rate/depth change;
- pitch-drift change;
- formant / spectral-envelope displacement;
- unvoiced consonant and breath alteration;
- transient/onset smearing;
- output level change;
- added latency and CPU;
- discontinuities / clicks / zippering.

Listening labels should stay separate from metrics:
- intonation;
- naturalness;
- expression preserved;
- consonant/breath quality;
- timbral damage;
- artifacts;
- musical preference;
- whether correction is audible when it should be transparent.

## Cross-processor test chain

Once a pitch-corrected reference is chosen, retain both the dry and corrected versions. Later modules should be tested with a matrix such as:

`dry -> module under test`

and

`chosen pitch-corrected reference -> module under test`

When interactions matter, extend deliberately:

`dry -> pitch -> EQ -> dynamics -> saturation -> spatial`

Do not silently replace earlier renders. Every stage should remain independently bypassable so an artifact can be traced to the subsystem that introduced it.

## External baselines

Use commercial tools as benchmarks, not targets to clone. Record versions and settings. MAutoPitch is useful as a simple automatic baseline; Melodyne-class editing is a stronger reference for independent pitch-center, modulation/drift, sibilant/unvoiced handling and formant-aware editing. Other tools Cody owns should be added to the matrix rather than excluded to protect Monkey's Ear.

## Blind comparison

When practical, randomize render names and loudness-match before listening. Do not reveal which render is Monkey's Ear until ratings are recorded. Repeated preference on multiple excerpts matters more than one striking example.

## Level matching

Avoid declaring a processor better merely because it is louder. Preserve peak/RMS/LUFS measurements as appropriate and make a matched listening set when comparing quality. Keep the unmatched production render too if level change is itself part of the intended effect.

## Failure corpus

Interesting failures become future test material. Preserve short excerpts for:
- breathy notes;
- growls/fry/distortion;
- very low/high F0;
- fast note transitions;
- wide vibrato;
- slides/portamento;
- consonant-heavy phrases;
- quiet/noisy passages;
- doubled vocals;
- inharmonic or effect-heavy sources.

A mechanism is not robust because it passes a clean sustained vowel.

## Promotion rule

Engineering proxies may reject a bad mechanism but cannot prove it sounds better. Promote a subsystem change only when it either:
1. improves a defined objective without unacceptable collateral damage;
2. wins controlled listening for a defined musical purpose;
3. enables a genuinely distinct user-controlled effect.

Keep negative results. They prevent the same failed idea from returning under a new name.
