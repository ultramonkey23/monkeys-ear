# Monkey's Ear Mobile — External Benchmarks and Lab Research Intake

**Research check:** 2026-11-08; URLs below are external primary documentation where available. This file records evidence and decisions for the current mobile product expansion, not a separately authoritative product roadmap. Product intent belongs in [Mobile vision](MOBILE_DAW_PRODUCT_VISION.md); integration/proof in [Technical integration](MOBILE_DAW_ENGINEERING.md). Internet/product-document claims are not tests or user-acceptance proof.

## 1. Known creator intent (not inferred from competitors)

Cody directs: a standalone **Android DAW**, **touch-first**, pattern/sequence/loop **and** live; external MIDI and ordinary physical keyboard play; deep quantization; touch piano roll and good alternate sequencers; sound and preset exchange with Monkey's Ear VST3 in REAPER; synchronized **stems** as the editing bridge; **AI optional, never required**; Caustic is an inspiration not a clone. **Experimentation is part of full production**, not a side prototype organization. All canonical mutation converges on `master`.

Creative influences provided by Cody include extreme/death metal, underground hip-hop, RPG/roguelite music, anime/manga and horror. They are **inspirations and stress tests**, not mandatory genre limits or borrowed intellectual property.

## 2. Competitive observations — copy the question, not the interface

| Reference | Source-confirmed behavior | Monkey's Ear lesson | Boundary |
| --- | --- | --- | --- |
| **Caustic 3** | Rack/machines; synth and BeatBox family; patterns and song sequencer, automation, samples, MIDI and effects; mobile-first real-time workflow. | Immediate rack sound-to-pattern flow and distinct instrument identities. | Don't clone its exact UI, machine roster, proprietary assets or arbitrary limitations. |
| **Koala Sampler** | Sample/sequence/perform surfaces; pad capture and resampling, sequence merging, effects, mixer, piano-roll options. | Capture -> chop -> play -> resample should be a short physical gesture loop. | Not an instruction to make sampling the only production paradigm. |
| **SunVox** | Modular synth and tracker-style pattern sequencing across many devices. | Dense deliberate pattern logic and flexible musical timing can coexist with nonstandard instruments. | Don't force tracker syntax on musicians who prefer touch piano roll. |
| **G-Stomper Studio** | Live pattern/song arranger, swing/meter controls, automation, multi-format sample support, track-by-track WAV and MIDI export. | Pattern/live/DAW interchange is a real Android benchmark, including non-4/4. | Need stronger own identity, not feature-count imitation. |
| **FL Studio Mobile** | Touch piano roll, automation, recording, mixer, sound packs, mobile/desktop project compatibility and file export. | Piano roll and arrangement must be deep production instruments rather than afterthoughts. | Its specific FLP parity is a different product decision: Cody selected stems + assets/presets. |
| **Drambo (iOS reference)** | Modular racks, clip launcher, arranger, multiple sequencers, parameter locks and morphing. | Distinct simultaneous editing paradigms and deep parameter control can serve advanced players. | iOS app is a concept benchmark, **not** an Android solution or implementation dependency. |

Primary sources:
- Caustic: https://apps.apple.com/us/app/caustic/id775735447
- Koala official manual (Sample/Sequence/Perform): https://manual.koalasampler.com/mobile/2-general-overview/
- SunVox official: https://warmplace.ru/soft/sunvox/
- G-Stomper Google Play: https://play.google.com/store/apps/details?id=com.planeth.gstomper
- FL Studio Mobile official: https://www.image-line.com/fl-studio-mobile
- Drambo App Store: https://apps.apple.com/us/app/drambo/id1469365718

## 3. Platform evidence, constraints and hypotheses

**Android audio:** Google's Android audio guidance recommends **Oboe** for the lowest practical latency, with low-latency/exclusive **requests**, callback-driven output, native rates/buffer tuning and no blocking/allocation on callbacks. Exclusive mode and low latency **are not guaranteed**. Android route, OEM implementation and Bluetooth must be measured. Oboe is therefore a serious candidate for adapting the existing C++ core, not a basis for claiming Android readiness. Sources: https://source.android.com/docs/core/audio/latency/app and https://developer.android.com/games/sdk/oboe/low-latency-audio and https://github.com/google/oboe/blob/main/docs/FullGuide.md

**Android MIDI:** Android MIDI supports USB, BLE and virtual transports subject to device/platform support; Android 13 adds USB MIDI 2.0 and Android 15 adds virtual MIDI 2.0. `MidiManager` and NDK `AMidi` are viable investigation surfaces; timestamp handling and hot-plug remain real engineering work. Sources: https://source.android.com/docs/core/audio/midi and https://developer.android.com/reference/android/media/midi/package-summary and https://developer.android.com/reference/android/media/midi/MidiReceiver

**Physical keyboards:** the supported musical path uses hardware `KeyEvent`, not a normal onscreen IME. Repeated key-down, missing/canceled key-up, keyboard layout/rollover and app focus need product handling. Source: https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/commands and https://developer.android.com/reference/android/view/KeyEvent

**Touch correctness:** Android recommends touch targets of **at least 48dp** for interactive elements; high note density needs zoom/fine-edit affordances rather than simply shrinking hitboxes. Source: https://support.google.com/accessibility/android/answer/7101858?hl=en

**Portable storage:** Android Storage Access Framework offers user-mediated open/create/document-tree flows, which are a useful basis for WAV stems and pack exchange without invasive blanket file permissions. Source: https://developer.android.com/training/data-storage/shared/documents-files

**REAPER:** selected-track stem rendering and consolidate/export workflows are supported; ReaScript/Lua can interact with media, tracks and tempo. This makes a future tested stem-import adapter possible without requiring `RPP` project translation. Sources: https://www.reaper.fm/sdk/reascript/reascripthelp.html and https://reaperaccessibility.com/wiki/Special:MyLanguage/The_render_and_Consolidate/Export_Tracks_Dialogs

**Network tempo-sync option (not a requirement):** Ableton Link's source implementation coordinates tempo/beat/phase across apps; its dual GPL/proprietary licensing means proprietary integration requires license review. Its existence is **not** a reason to insert Link into the mandatory engine or make offline sessions depend on networking. Source: https://github.com/Ableton/link

## 4. Cody's existing research and product work — reuse it

This product already has distinctive musical mechanisms and research; mobile must not be defined as a fresh generic sampler:
- **Verified in inspected source shape:** C++20 instrument/FX core, `PresetData` and text serialization, Sound Space, ChronoState, filters, tube/chassis coloration, oscillators and sub weight, tempo-movement, VST3 editor and shared ecosystem contract. `PERFORM` has DRIFT/FRACTURE control in the REAPER-facing editor. These are candidate mobile sound engines/gesture vocabulary after platform proof, **not already mobile features**.
- **Historical Lab research evidence:** `ultramonkeydog-lab/research/audio/monkeys-ear/2026-09-15/AUDIO_STACK_PRODUCT_FOCUS_REASSESSMENT_001.md` identifies REAPER/human audio comparison as the important promotion surface, and Hydrogen, Drumlabooh and GMD/Mido as targeted rhythm/groove **external reference bodies** rather than copied product implementations.
- **Historical timing and artifact discipline:** `ultramonkeydog-lab/research/audio/AUDIO_STACK_EVOLUTION_20260912.md` reports a deterministic MIDI round-trip fixture and recommends preserving MIDI ticks, tempo maps and raw identity. This is time-scoped research, **not proof that the new mobile event model exists**.
- **Monkey's Ear EQ/pitch research:** `research/onion-skin-eq/ARCHITECTURE.md` favors explicit user-controlled Preserve/Yield/Equal relations, bounded effects and subjective listening over automatic overlap elimination; `research/pitch-correction/ARCHITECTURE.md` distinguishes center, drift, vibrato and transition. These suggest expressive editing semantics; neither research architecture is evidence of mobile feature implementation.
- **Research ownership:** Lab True Scholar should research only a named Monkey's Ear problem and produce candidates for existing Code Prime, not establish a second audio prime, independent product vision or scheduler. Historical source: `ultramonkeydog-lab/research/notes/TRUE_SCHOLAR_MONKEYS_EAR_AUDIO_RESEARCH_20260914_001.md`.

Keep provenance: **CREATOR INTENT**, **CURRENT SOURCE SHAPE**, **EXTERNAL DOCUMENTATION**, **DATED LAB RESEARCH**, **INFERRED CANDIDATE**, **RUNTIME PROOF** and **HUMAN APPROVAL** are separate claims. Research results must not silently become production engine mandates.

## 5. High-value distinctiveness hypotheses to playtest in production

1. **The musical rack becomes a performance instrument:** expose familiar fast instruments plus deep DRIFT/FRACTURE and Sound Space interactions at expressive touch scale; whole-patch automation is editable and recallable.
2. **Groove is not a forced correction:** raw performance and quantized intention coexist. A drummer can intentionally make metal hits machine-tight and a producer can intentionally leave underground hip-hop drums late; neither should be silently normalized to a default.
3. **Sound design directly becomes arrangement:** capture a noisy feedback gesture or resonator swell, resample it to a pad, slice/sequence it, automate it across an odd-meter scene, and stem-export it to REAPER with the sound intact.
4. **Composed tension, not automatic AI taste:** chance, drift, accents and mutation should be controlled by the musician and reproducible/undoable; unstable horror effects need stable project recall.
5. **The mobile instrument is a partner, not a toy remote:** complete offline sessions and deliberate sound design on the phone; detailed audio editing in REAPER through aligned stems, with portable compatible sounds/presets.

**Dispositions:** ADOPT creator scope and portable audio/quantize proof requirements; EXPERIMENT with touch layouts, rack/performance gestures and advanced rhythmic transforms inside canonical production; DEFER framework, cross-platform UI, plugin hosting and Link commitment until needed by current consumers; REJECT mandatory AI, invented preset equivalence, separate prototype repositories and non-`master` integration.

**Open, deliberately unresolved:** Android supported API floor; Kotlin/Compose vs custom rendering hybrid; reusing C++ DSP without regressions on ARM; sample/clip streaming engine and effects architecture; internal event schema/time resolution; Android background/audio focus; sample formats/license packaging; REAPER import assistant UX; MIDI 2/MPE depth; actual physical touch latency; sound quality and creative preference.
