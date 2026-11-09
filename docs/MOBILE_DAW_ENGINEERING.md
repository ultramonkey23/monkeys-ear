# Monkey's Ear Mobile — Technical Integration and Real-Production Proof

**Role:** implementation engineering companion to [Mobile product vision](MOBILE_DAW_PRODUCT_VISION.md). This is a **target design and verification contract**, not a report that Android code, a working sequencer, or interoperability already exists.  
**Branch / mutation:** canonical `master`, existing Lab project owner and Code Prime for Lab-routed product mutation. No separate mobile product repository, disposable branch or PR.  
**Source inspection basis:** Monkey's Ear GitHub `master`, inspected 2026-11-08; local uncommitted work, running Windows REAPER, live Lab execution, physical Android devices and active agent sessions were NOT accessible.

## 1. Functional boundaries and reuse before invention

The currently inspected C++20 `monkeys_ear_core` comprises voice generation, live audio input, multi-pass filters, tube rack/drive, cabinet resonator, Sound Space, ChronoState, EQ, delay, reverb, safety limiter, presets and module/ecosystem types. Host wrappers are in `vst3/`; `CMakeLists.txt` links a common core to the main VST3 and vocal module. `include/monkeys_ear/preset.h` contains `PresetData`, `PresetManager` and factory-patch constructors; `src/preset.cpp` serializes named `KEY=VALUE` fields into `.mepreset`-style text. `vst3/preset_param_snapshot.h` performs host parameter projection. The two representations are **not interchangeable merely because they refer to the same patch**.

**Investigate / retain** the existing cores first. Do not fork the C++ DSP for mobile or claim it runs on Android until it compiles and survives Android real audio-device measurement. A likely integration arrangement, not an implementation decision made on the Lab's behalf:

```text
Android touch / USB-BLE MIDI / physical keyboard / audio input
     -> platform timestamp + controller adapters (off audio callback)
     -> one musical event and transport vocabulary
     -> song / scenes / patterns / loops / arrangement state
     -> bounded scheduler (sample-frame domain, RT-safe handoff)
     -> existing proven DSP where reuse fits + needed instrument/sample voices
     -> routing / effects / mixer -> Android low-latency audio device
                               \-> offline bounce / aligned stems / soundpacks
REAPER VST3 <-> compatible sound/preset interpretation + stem bridge
AI tools (optional) -> proposed editable events/assets only; never mandatory
```

The **same authored event** must have consistent meaning in touch piano roll, step editor, live launch and arrangement; do not build three competing clocks, duplicate note stores, or a second unrelated preset engine.

## 2. Transport, musical time and sequencing correctness

Keep separate:
- **Monotonic input time:** timestamp from touch, hardware key or MIDI transport, with source/clock provenance.
- **Musical time:** beat/tick and meter/bar position, tempo map and pattern/clip offset. Avoid float seconds as the only source of note identity. A high-resolution tick scheme is a candidate; validate resolution against microtiming and tempo changes before freezing it.
- **Audio time:** integer sample frames on the active render/stream clock, with loop wrap and deterministic scheduling. Do not quantize by blindly rounding frame positions across tempo changes.
- **Visual time:** UI gestures and edit previews, never authoritative for real-time playback.

The event representation should preserve stable note/event identity, pitch, velocity (including input devices that cannot express it), note-on/off, channel/expression, pattern reference, track and origin/raw timestamp as appropriate. Note-off, pedal/sustain, loop boundary, tempo map, polymeter, sample tails and automation must have explicit behavior. On-screen editing and live recording share undo/redo and recall discipline. Variable tempo/meter and odd-meter patterns must not silently revert to 4/4 assumptions. Align sample-accurate scheduling to event frame offsets whenever the existing engine/host path permits it.

## 3. Quantization as editable musical intention

Support coherent, non-destructive states for **OFF**, **monitor/input quantize**, **post-record quantize** and **playback quantize**, each with provenance and explicit scope. Capture the original timing before any correction; preserve a reversible note/event source so changing grid, strength or groove never repeatedly re-quantizes an already rounded result.

Quantization parameters and planned behavior:
- **Grid:** free, note-value divisions (straight/triplet/dotted where meaningful), bar/beat-relative or pattern-specific; compare odd meters and changing tempo. Optional custom groove template as a bounded timing map.
- **Strength:** blend raw time toward the selected target rather than always hard-snapping; the value must be editable after recording.
- **Swing/groove:** phase/time displacement with controlled onset order; apply to a clearly defined subdivision and do not shift unrelated notes as an invisible global default.
- **Note durations:** specify whether to move onsets alone, preserve lengths, quantize ends independently, or preserve legato ties. Never shorten notes to invalid or accidental zero duration.
- **Microtiming:** signed offsets/nudges at fine resolution, per-note and/or per-lane; make visible in piano roll and step view.
- **Per-gesture scope:** preview, compare with original, undo, choose note/clip/pattern/track scope. Force/override snap deliberately.
- **Live safety:** retrospective changes must not reorder events already emitted into an active audio callback; late-input policy should be explicit, especially for external Bluetooth devices.
- **Rhythm vocabulary:** drums/hat rolls, double-kicks and blastbeats, swung/off-grid hip-hop, triplet fills, odd-meter metal riffs, unstable horror tension and steady game-loop ostinatos are distinct acceptance fixtures, not genre-locked templates.

**Candidate advanced controls to earn by use:** per-step probability/seeded reproducibility, ratchets/substeps, conditional triggers, per-step parameter locks, independent lane lengths, Euclidean/accent generators, tempo-ramp performance, nondestructive groove capture from human MIDI, velocity-aware quantize and deliberate timing instability. Expose mechanisms explicitly and reject novelty that makes a musician less in control.

Proof includes round-trip of untouched source events; idempotence of preview/non-destructive application; deterministic replay with chosen seed; no missing/extra note-offs or stuck notes after loop wrap; exact user-visible result at 0%, 50%, 100% quantization; and real played performance review, not just mathematical test vectors.

## 4. Performance input adaptation

**USB/BLE MIDI:** use Android-supported MIDI services (`MidiManager` and/or NDK `AMidi` based on discovered API/device envelope) with hot-plug, lost devices, note-off recovery and timestamp handling. Android 13 introduces USB MIDI 2.0 capability; Android 15 introduces virtual MIDI 2.0 support; actual devices/features vary. A MIDI 1.x working path is still required. Sustain, pitch bend, controller input, expressive velocity and channel mapping are separate requirements to verify.

**Physical typing keyboard:** accept hardware `KeyEvent` down/up, not Android's software IME (soft keyboards are not guaranteed to emit musical key events). Editable musical layout, octave shifts, key bindings, held/repeated notes, chord rollover, sustain, default velocity/velocity modifier and focus-aware typing safety. Prevent accidental notes while renaming a track or searching sounds. Handle canceled events, repeated downs and focus loss with predictable note-offs. USB and Bluetooth keyboards may differ in timing/rollover; measure rather than assert equivalence to a MIDI keyboard.

**Touch instruments:** pressure/slide/multitouch where supported, hit targets appropriate for fingers and accessibility, fast audition/record, velocity alternatives (gesture position/hold/slider) and visible captured event timing. Touch performance must never be treated as the sole rhythmically precise way to play drums; quantization and physical input are required peers.

**Platform audio:** Oboe (C++ wrapper over AAudio on supported versions, OpenSL ES fallback) is a strong implementation candidate, not an installed dependency or guarantee of low latency. Request low-latency/exclusive where available, use callback-driven audio, avoid forced non-native sample rates, handle stream disconnect/routes and device-specific buffer/rate changes. No allocations, locks, filesystem calls, UI or optional AI inference in the audio callback; use bounded, nonblocking event queues with explicit overflow/all-notes-off behavior. Judge practical latency, xruns, thermal behavior, battery, device-route switch and microphone/USB recording on real hardware. Bluetooth *audio output* latency and Bluetooth MIDI delivery latency are distinct; do not make universal timing claims.

**Files / privacy:** projects, stems and sound packs work locally/offline. Use Android's user-mediated Storage Access Framework or other currently supported APIs for import/export, avoiding blanket storage permissions. Microphone capture requires appropriate user consent and permission; no automatic uploading of sessions or samples.

## 5. Stem bridge and compatible sounds

**Stem interchange is the creator's selected primary audio-edit bridge.** It is not an excuse to require REAPER's editable arrangement schema on mobile. First production handoff should produce REAPER-loadable audio regardless of whether any shared preset engine is present.

### Stem bundle candidate contract — schema to be proven, not deployed yet

A versioned self-contained export contains:
- human-readable song/project title, unique export identifier and source app/build identity;
- tempo/meter map, timeline origin, optionally section/region markers and musical loop boundaries;
- file format/sample-rate, total render frame range, identical **origin/frame-zero** for every stem, common duration or well-defined tail extension, and explicit silence padding so **all stems line up without hand nudging**;
- track/stem ID, stable display name, ordering, role, mono/stereo channel count, duration in frames, checksum and relative safe filename;
- render state: printed effects vs dry, bus routing and effects-tail handling, bypass/solo/freeze decisions, latency compensation, normalization/limiter choices and source frame offset; make it possible to avoid inadvertently double-applying REAPER effects;
- asset provenance for any bundled user samples; never embed unlicensed commercial content without rights;
- an editable marker/tempo map sidecar if useful, but **do not make decoding a proprietary metadata file mandatory just to load WAV stems**.

REAPER conformance starts with importing WAV stems at an identical time zero, checking relative sample-frame alignment, durations, non-4/4/tempo-map behavior and tails. Host import helpers may be added only when they consume the *same* stem contract; do not invent a separate ReaScript product or another project authority.

### Presets and soundpacks

Treat `PresetData` field semantics and `vst3/preset_param_snapshot.h` as existing sources, and preserve stable IDs and legacy patches. Define a compatible **versioned portable preset envelope** with engine/module identity, source format/version, required DSP capabilities, referenced samples and feature/dependency manifest. Do not directly serialize C++ memory, invent lossless mappings for unsupported parameters, or claim identical renders without A/B on both platforms. Track `portable`, `translated-with-known-loss`, `render-only` or `unsupported` outcomes explicitly. A sound pack can be samples + metadata + compatible patches; its ability to load does not imply that every sound engine exists on Android.

Offline rendering and transfer are not AI services. Both platforms can work without network. File names, imports, ZIP/paths and malformed state must be validated transactionally with safe rollback to the previous working project.

## 6. Dependency-led production sequence (not disposable prototypes)

No date promises; order is by real consumers and verified existing capability. These are overlapping **production capabilities**, not separate products or throwaway projects. Code Prime and live Lab routing own implementation planning/execution.

1. **Recover whole current implementation:** inspect C++ DSP/presets, host parameter/event paths, tests, local uncommitted changes, device/toolchain, active agent ownership and user-approved sound. Reconcile with this target; preserve existing REAPER release work.
2. **Establish shared timeline/event and portable asset contracts:** prove note events, patterns, tempo map, recall and preset compatibility boundaries before multiple input UIs assume different semantics.
3. **Prove actual Android audio use of existing core:** native audio callback, playable Monkey's Ear patch, audio device lifecycle, input route and physical latency/xrun instrumentation. Reuse DSP if proven; repair actual issues without splitting engines.
4. **Make live input and sequencing real:** physical keyboard + USB MIDI + touchscreen keys, playable drum and melodic steps, touch piano roll, pattern/loop transport and non-destructive groove/quantize. One musical project state consumed by the audio renderer.
5. **Connect full song and performance:** launchable scenes/pattern variation, arrangement, overdub, resampling, audio capture, mixing/automation and resilient save/reopen. Follow actual composing sessions; do not postpone essential functions merely because they are hard.
6. **Deliver the REAPER bridge:** reusable asset and preset compatibility, file-based stem bundles, real cross-app import and return audio flow; verify against REAPER VST3 and human-edited audio.
7. **Prove musician usefulness / system behavior:** real phone and tablet sessions, external keyboards/controllers, multiple devices and thermal states, saved project recall, offline operation, accessible touch editing, sound-quality listening and Cody's acceptance. Update actual release gates only as proof warrants.

Do not rename these stages `non-production prototypes` or move them to branches. Early vertical work remains valuable product work **if it stays integrated and grows into the final capability**.

## 7. Verification witnesses (none asserted completed by this document)

| Area | Machine-checkable evidence | Real-world consumer proof |
| --- | --- | --- |
| Raw MIDI & keyboard | stable event IDs, note-on/off, sustain, timestamp/repeat/focus safety | playable, responsive keyboard and MIDI devices on Android |
| Touch editing | gesture state/undo tests, hit target checks, note move/resize/velocity roundtrip | musician piano-roll/drum/arrange session on phone and tablet |
| Sequencer & live | tempo/loop/polymeter scheduling, scene switching, recall | live performance/overdub replay without lost or stuck notes |
| Deep quantize | raw/quantized reversibility, groove/strength fixtures, tempo maps | usable blastbeat, chopped hip-hop, odd meter and expressive takes |
| Audio | RT callback instrumentation, xruns, DSP finite output, stream recovery | physical latency/headroom/thermal/route-switch proof |
| Sounds & presets | version/semantic validation, actual load/translation and checksum | recognized patches and accurate sound comparison in REAPER/mobile |
| Stem bridge | per-stem start/frame/rate/header/metadata proof, deterministic render | REAPER import with audible synced stems; edits, exports, reimport |
| Offline/no AI | disable data/model/network and run tests | full music session, save, load and render offline |
| Creative quality | comparison fixtures and level matched audio | creator judgment, retained presets/songs, not automated aesthetic scoring |

**Release truth:** the inspected remote repo does not establish a compiled Android app, REAPER stem round-trip or touch device test. Current local work/active agents remain UNKNOWN. This contract is a target for the existing production process.

## Executed Wave 1 — shared pattern/quantize to existing audio engine (2026-11-08)

**Implementation in production source (not a throwaway proof-of-concept):**
- `include/monkeys_ear/pattern_transport.h` now defines `PatternNote` with persistent raw timing/identity, deliberate per-note nudges, configurable tempo/PPQ/loop/grid/strength/swing; `PatternProgram` compiles validated, fixed-capacity, immutable note-on/off events and preserves the previously valid program on rejected input. No MIDI event or project edit modifies the original recorded onset.
- `PatternPlayer` renders compiled events through **the current `MonkeysEarEngine` MIDI note-on/off and audio block APIs**, splitting blocks exactly at event frame positions, replaying loops with note-off ordering, passing input audio pointers through, and allowing DSP effect tails when playback stops. It has no heap/locks/I/O inside `process()`.
- `test/test_pattern_transport.cpp` exercises quantization strength/swing/nudges, sample-frame timing, duplicate/invalid input rejection, odd-meter loops, block-size-independent playback, audio input pointer continuity, stopping and loop wrap; the full CMake target also runs actual sound through existing `MonkeysEarEngine` on a local build. `run_tests.bat` requires the new target rather than conditionally skipping it.
- The project uses `-ffast-math` globally; initial ordinary `isfinite` input checks were falsified by an optimized local test. Production code now uses bit-level finite validation before numeric timing conversion.

**Actually executed in the isolated build container:** standalone production header and event-engine test compiled and PASSED with GCC C++20 `-O3 -ffast-math -Werror`, and GCC AddressSanitizer/UndefinedBehaviorSanitizer. That is real **sequencer timing execution** with a deterministic fake event/audio consumer; it is NOT a complete Android build or REAPER host result.

**Evidence boundary:** a GitHub source commit and test registration do not prove the full `monkeys_ear_core` CMake/Windows build executed. The CMake integration test using actual `MonkeysEarEngine` is present but execution on Cody's machine/Windows REAPER or Android hardware remains UNKNOWN until the actual installed Lab runs it. No touch piano roll, Android transport, multitrack arrangement, variable-tempo map, human playing or stems implementation is claimed by this wave.

**Next consumer pressure:** schedule edits from physical keyboard/MIDI/touch into the same canonical raw-note model, add first-class pattern/scene arrangement and richer reversible quantization, and produce an actual WAV/stem render accepted by REAPER. Do not create a different sequencer when implementing mobile input adapters.

## Executed Wave 2 — aligned 24-bit audio stems and portable manifest (2026-11-08)

Production source now owns a **single** `include/monkeys_ear/wav_writer.h`; prior `test/wav_writer.h` forwards to it to preserve existing test callers rather than maintaining a divergent WAV implementation. The writer streams bounded 4,096-frame batches into standards-compatible stereo PCM24 RIFF (not RF64) and can silence-pad a source at an explicit offset into a **common timeline frame zero**. It validates sample finiteness with bit-level IEEE checks even under `-ffast-math`; rejects invalid ranges; and emits a labelled **non-cryptographic FNV-1a64 data checksum**, not a security digest.

`include/monkeys_ear/stem_bundle.h` now exports a prevalidated group of source buffers into a newly created directory containing `stem_01.wav`, etc., plus a JSON `monkeys-ear-stems` v1 manifest with frame count, sample rate, static BPM/meter, source offsets, track names, printed-effect state and per-file data checksums. A failed export cleans up **only** the directory it successfully created and never overwrites an existing directory. Invalid UTF-8 names are rejected. Silent tracks remain valid and aligned. This tool is *offline-only*: file access is forbidden on the audio callback.

`test/test_stem_bundle.cpp` verifies alignment/silence, WAV PCM headers/packing, clipping, sample rate, 7/8 metadata, nonfinite rejection, UTF-8 rejection, rollback and existing-file protection; CMake's integrated build additionally renders **two different factory sounds through PatternPlayer + the existing MonkeysEarEngine**, then writes two different aligned stems. `run_tests.bat` requires the integrated executable.

**Real local evidence:** the portable exporter test PASSED under GCC C++20 `-O3 -ffast-math -Werror` and AddressSanitizer/UndefinedBehaviorSanitizer; independently, Python's built-in `wave` decoder and `json` parsed the generated artifacts, confirmed both WAVs share frame zero/duration/sample rate, and recomputed both FNV checksums from PCM bytes. Those are **actual generated files**, not just mocked header assertions.

**Explicit limits:** this exports already rendered audio buffers. It does **not** supply Android multitrack recording, a REAPER import helper, variable tempo maps, RF64/greater-than-4-GB WAV, song-length effect-tail policy or physical REAPER stem round-trip yet. The full existing-engine integration test is source-registered but was **not executed on the Windows/REAPER target** from this session; release/readiness remains UNKNOWN there.

**Follow-through dependency:** next production waves should connect edit/sound/pattern track state to offline rendering, preserve sample/FX tail semantics across tracks, test a true REAPER import, then extend the compatible sound/preset pack and Android touch/MIDI input. No parallel DAW project or mandatory AI layer.

**Release NDEBUG regression (same wave):** The two new C++ probes now explicitly re-enable assertions in their own translation units even when a Release build supplies `-DNDEBUG`; otherwise their tests could falsely pass without evaluating conditions. Both portable probes were rerun with `-O3 -DNDEBUG -ffast-math -Werror` and with AddressSanitizer/UndefinedBehaviorSanitizer, and passed in the isolated execution environment. This does not extend proof to Android or REAPER.
