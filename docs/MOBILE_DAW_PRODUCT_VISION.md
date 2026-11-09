# Monkey's Ear Mobile — Touch-First Standalone Android DAW Product Vision

**Authority:** Cody's stated product direction, 2026-11-08.  
**Status:** Accepted **future product intent**, **not** an assertion of implementation, platform qualification, latency measurements, or a released Android DAW.  
**Integration:** Monkey's Ear family; preserve the existing REAPER VST3 modules and Lab-owned workflow; canonical development branch: `master`.

## 1. The product, not a side prototype

Monkey's Ear will gain a self-contained Android app and evolve into an unconventional **mobile DAW**, taking inspiration from Caustic's immediate instrument/pattern workflow while developing Monkey's Ear's own musical identity. **Touch-first, not touch-only**. It should be useful alone and become more capable when paired with the Monkey's Ear REAPER ecosystem. It is **not an AI-requiring DAW**: music creation, live performance, editing, sequencing, rendering and asset handling must work without a model, AI service, or AI network dependency. Optional AI may improve workflows but may not replace manual capabilities.

**Production philosophy:** for this project, experimentation and prototyping are **part of full production**, not a separate non-production track. Keep the evolving product, tested ideas, prototypes and their truthful maturity/proof states visible in the same canonical production body. Do not split into speculative app repositories, throwaway proof-of-concept products, feature branches or disconnected alternate implementations merely because an idea is under exploration. An experimental feature in production source is not automatically accepted, user-tested or release-ready; represent that difference with evidence and appropriate feature exposure, not divergent project identity.

## 2. Required musician workflows

### A. Performance input — complementary, not mutually exclusive

- External MIDI keyboards, drum controllers and other supported MIDI instruments, including appropriate MIDI note, velocity and timing capture.
- Touch performance for melodic and expressive interaction, virtual keys and pads, and editing.
- **Ordinary physical typing/computer keyboard** as a configurable playable instrument for users without MIDI equipment (for example, USB/Bluetooth keyboard routes available to the Android device). Define octave, note layout, sustain, repeat behavior and useful velocity/expression alternatives because ordinary keys do not convey velocity.
- Avoid pretending touch drum-pad timing equals the physical precision of a MIDI controller or conventional keyboard. Real-time input compensation, good tactile/visual feedback and deep quantization must make touch viable without misrepresenting its limits.
- Retain intended musical notes/timing consistently across input surfaces; validate device-specific Android keyboard focus, key rollover, Bluetooth latency and input APIs rather than claiming universal hardware behavior.

### B. Touch piano roll and sequencing

- A first-class, responsive **touch piano roll**: fast note creation, move/resize, velocity/expression editing, multiselect, copy/duplicate, scale/grid context, pan/zoom and undo, with fine control that does not depend on mouse-style precision.
- Strong **step/drum and melodic sequencers**, with useful per-step dynamics and editable pattern length. Multiple sequencing approaches should coexist rather than forcing every musical task into one grid.
- **Patterns, loops, clips/sections and arrangement**: create, repeat, chain, vary, trigger and arrange musical material; move fluidly between building a loop and producing a song.
- **Live performance/recording**: capture takes, overdub, loop recording and perform/launch patterns while keeping material editable. Decide exact performance/arrangement model through musician-led experiments, not by cloning another DAW.
- Candidate expressive options to evaluate inside production, *not yet frozen requirements*: per-step probability, ratchets, ties, polymeter, per-step automation, pattern morphing and live pattern switching. Judge by musician control and actual sound/workflow usefulness.

### C. Deep, musician-controlled quantization

Quantization should work for live capture, touch input, hardware keyboard input, MIDI controller performance and subsequent piano-roll/sequence editing:

- Configurable off/on, input-time and after-recording quantization; full, partial/strength-based and non-destructive workflows.
- Useful beat/bar grids and subdivisions, including straight/triplet/dotted choices where musically applicable; swing, groove templates and user-adjustable timing feel.
- Independent timing controls where meaningful (note-on, duration/note-off, and pattern/track scope); microtiming/nudges and explicit snap overrides.
- Preserve the unquantized performance or an equivalent reversible source so artists can recover expressive timing; expose previews, compare and undo.
- Resolve audio/device input latency separately from intentional groove and quantization offsets. Do not hide Bluetooth/hardware limitations with misleading timing claims.
- Exact grids, groove representation, compensation strategy and implementation performance remain design/measurement questions; test with live musicians and actual devices.

### D. Touch is the primary UX authority

Large usable controls and an immediate, fluid instrument/sequencer workspace are higher priority than copying the REAPER interface. Provide gestures for play/stop, scrub, grid editing, zoom, navigation, mix and arrangement, but avoid relying on undiscoverable gesture-only actions; important functions need explicit accessible controls. Tune separately for phone and tablet density/orientation. Support physical keyboards and MIDI without forcing mobile touch interactions on them. Preserve expressive depth behind approachable primary controls rather than deleting deep options.

## 3. REAPER compatibility and interoperability

**Creator-selected bridge:** **shared sounds and presets + audio stems**. Full bidirectional *editable DAW project* conversion is **not required** by the current product direction.

1. **Sound exchange:** mobile and REAPER Monkey's Ear can use compatible samples, recorded sound assets and sound packs, with explicit formats, source references, validation, content ownership and safe import/export. Keep data portable and local-first.
2. **Preset exchange:** define versioned instrument/effect presets and translations where DSP/control semantics genuinely match. A preset file's portability does **not** by itself guarantee identical sound if platform engines or effects differ; document/render/verify parity and handle unsupported parts explicitly. Preserve existing REAPER/VST3 preset and stable parameter meanings.
3. **Stem bridge:** export separate musical parts/tracks as **synchronized audio stems** intended for import to REAPER for detailed audio editing. Include an unambiguous common start/timeline offset, duration/alignment, sample rate/format, tempo and meter where applicable, and identify effect/tail treatment. Define dry/wet, naming, clipping/headroom and render options during production implementation, and test repeatable REAPER import/alignment.
4. **Return paths:** allow importing rendered/edited audio into mobile where musically relevant; do not imply editable reconstruction of an original MIDI pattern from a rendered stem.
5. **No imposed shared UI or hard project parity:** the two environments may present and edit material differently. A richer compatibility path can be considered later if Cody chooses it, but it must not displace the stem-based workflow or break stable host behavior.

## 4. Engineering and integration boundaries

- **Inspect before design:** trace the current C++ DSP, presets, VST3 state and host paths, dependencies, audio thread constraints, existing tests, platform boundaries and Lab projects before choosing mobile engine reuse or new Android adapters. A missing Android app is not proof the audio engine must be rewritten.
- **Hard real-time:** uphold the established zero-alloc/zero-lock/no-I/O audio-callback discipline and bounded failure/fallback properties. Mobile-specific latency and deadline targets must be *measured on target hardware*; desktop REAPER benchmark results cannot certify Android.
- **Optional AI as enhancement:** it may add suggestions or transformations, but cannot be on the mandatory audio/performance/sequence/recall path, and failure or disconnection must not interrupt core functionality.
- **Shared contracts, no duplicate ecosystems:** reuse compatible DSP, musical semantics and format contracts where evidenced; avoid separate competing preset identity/authority systems.
- **Lab ownership:** Code Prime and existing Lab mutation/provenance rules govern Lab-routed code. `master` is the sole canonical product branch; direct authorized maintenance likewise targets `master`. No side PR/branch or accidental reliance on GitHub's still-`main` default.
- **Evidence categories:** intended / source implemented / built / device exercised / musically playtested / REAPER interoperability verified / release accepted are different claims. Do not promote on test presence alone; prototyping remains real production work even before release acceptance.

## 5. Product acceptance witnesses to develop during production

A faithful future implementation will need to demonstrate: a working offline/AI-free Android session; responsive touch piano-roll and sequencer editing; MIDI/controller and physical computer keyboard pathways; real loop/pattern/live capture and replay; reversible precise quantization with groove/microtiming; usable production of a musical arrangement; reliable, correctly aligned stem exports imported into REAPER; and actually compatible sounds/presets transferred both ways where the participating engines support them. Device/user proof, runtime load and precise import alignment are required before claiming success.

**Present evidence boundary (2026-11-08):** the inspected GitHub `master` source and existing REAPER/VST3 contracts describe native C++ instruments/effects and preset machinery. This document authorizes the *product direction*; it does not claim an Android DAW, physical keyboard mapping, touch piano roll, multitrack stem exchange or live Android sequencing already exists. A separate local checkout, active agent work or deployed runtime may contain additional facts that this remote-only inspection did not observe.
