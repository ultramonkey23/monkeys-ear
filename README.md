# Monkey's Ear (ME-01)

> **Production-grade low-latency digital instrument and sound-design environment.**
> Combining pitch-locked weight construction, synthesis, live audio processing, multipass filtering, modulation, parametric EQ, amplification, delay, and reverb.

---

## First Playable Vertical Core

```
MIDI / MIC / LIVE AUDIO
           ↓
        SOURCE (Dual PolyBLEP + independent fundamental / 1:n sub weight)
           ↓
       WEIGHT / CHARACTER SPLIT (clean anchor + ChronoState body)
           ↓
        FILTER (dual-pass ZDF SVF: LP/HP/BP/notch/peak, serial/parallel/split)
           ↓
     DRIVE / TUBE (Causal Asymmetric Waveshaping with Dynamic Cathode/Sag Memory)
           ↓
    CAB / RESONATOR (4-Pole Acoustic Body / Modal Chassis Resonator)
           ↓
     PARAMETRIC EQ (4 bands: bell/shelves/HP/LP with smoothed modulation)
           ↓
         DELAY (Stereo Cross-Feedback Ping-Pong with 4-point Hermite Interpolation)
           ↓
   FDN / ALGORITHMIC REVERB (8-Line Feedback Delay Network with Orthogonal Hadamard Matrix)
           ↓
  OUTPUT & SAFETY LIMITER (Zero-Latency Signal Protection & Denormal Scrubbing)
```

---

## Real-Time Performance & Proof

Verified on Windows x64 @ 48kHz (Build with CMake + Ninja + GCC 14.2):

| Buffer Size | Block Deadline | Measured Avg Block Time | Measured Max Block Time | Deadline Margin | Deadline Misses |
|---|---|---|---|---|---|
| **32 samples** | 666.67 $\mu s$ | **13.84 $\mu s$** | 29.60 $\mu s$ | **97.92 %** | 0 |
| **64 samples** | 1333.33 $\mu s$ | **24.74 $\mu s$** | 45.00 $\mu s$ | **98.14 %** | 0 |
| **128 samples** | 2666.67 $\mu s$ | **94.97 $\mu s$** | 894.70 $\mu s$ | **96.44 %** | 0 |
| **256 samples** | 5333.33 $\mu s$ | **106.06 $\mu s$** | 189.80 $\mu s$ | **98.01 %** | 0 |

- **Reported Plugin Latency**: 0 samples
- **Memory Allocations in Audio Path**: 0
- **Locks / Mutexes**: 0
- **Audio Output Verification**: 48kHz 24-bit PCM stereo WAV targets rendered; focused tests cover finite extreme output, pitch ratios, filter/EQ response, modulation causality, and preset round-trip.

---

## 8 Performance Macros

- **Macro 1 (Brightness)**: Filter Cutoff Frequency (40 Hz to 18 kHz)
- **Macro 2 (Bite)**: Filter Resonance & Saturation Q
- **Macro 3 (Heat)**: Tube Drive & Saturation Wet Mix
- **Macro 4 (Body)**: Cabinet Modal Resonator Size & Depth
- **Macro 5 (Echo)**: Stereo Delay Time & Feedback
- **Macro 6 (Space)**: 8-Line FDN Reverb Room Size & T60 Decay
- **Macro 7 (Mic Blend)**: Live Microphone / External Audio Input Mix
- **Macro 8 (Character)**: Dynamic Cathode Memory & Thermal Sag

## Integrated creative controls — quick playtest guide

The instrument now has a **native Windows VST3 editor**, opened from the host's normal plug-in window. Start on its **PLAY** page rather than navigating 178 parameters. Select one of eight sounds, use the eight macros, shape the amp ADSR, then experiment with synced groove. **PERFORM** gives you a large two-axis DRIFT / FRACTURE gesture field that animates existing FM, filter, ChronoState, tube, subharmonic and Sound Space interactions. **KEYS** shows external MIDI setup and keyboard performance controls, and **DEEP** exposes the full engine with area selection and search, including four independent tube positions. Host changes use standard VST3 automation callbacks rather than a separate DSP control channel.

In PLAY, **Sound Palette** (ID 173) contains:
- `0 Current`, `1 MONOLITH`, `2 FERAL WOBBLE`, `3 VELVET LEAD` (unchanged legacy choices).
- `4 TUBE FOUNDRY` — four serial tube positions, modulation and cathode/supply interaction.
- `5 CLOCKWORK SUB` — two parallel tube pairs, subharmonics and tempo-divided movement.
- `6 GLASS CHAMBER` — long expressive envelopes, modal Sound Space and subdued nonlinear coloration.
- `7 SOUND SPACE` — original Sound Space factory demonstration.

The earlier **PRESET: Target Patch** control (ID 79) deliberately stays a four-option legacy selector: Current, MONOLITH, FERAL WOBBLE and VELVET LEAD. This preserves old project and automation meanings. **PLAY: Sustain Pedal** (ID 172) supports MIDI CC64, with held notes releasing only when the pedal rises. Sustaining multiple repeated notes remains polyphonic; host deactivate resets the voices. Host MIDI note-on/off timing and parameter automation are now scheduled by sample offset within each audio block. The host accepts no-audio parameter flush calls. The sustained pedal position is an ephemeral performance input and is never reopened as down on session restore.

For immediate playability, start with the existing eight macros **Brightness / Bite / Heat / Body / Echo / Space / Mic Blend / Character**, then adjust `AMP: Attack/Decay/Sustain/Release` (IDs 161..164), `FILTER ENV: Attack/Decay/Sustain/Release` (165..168), `LFO: Tempo Sync` (169), `LFO: Tempo BPM` (170) and `LFO: Subdivision` (171). **RHYTHM: Follow Host Clock** (174) is an optional DAW-tempo source for the *same* LFO. When enabled alongside Tempo Sync, a VST3 host that provides valid tempo drives the rate and a playing host with valid musical position re-aligns the cycle to its timeline. On host absence/invalid tempo it automatically uses the patch's manual BPM instead. Existing patches default to manual timing. **RHYTHM: Groove Swing** (175) warps the periodic waveform cycle from straight 50/50 toward 75/25, changing movement timing without changing the number of cycles per beat; sample-and-hold deliberately retains one independent random event per cycle. The 12 divisions are 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, quarter/ eighth triplets and dotted eighth/sixteenth.

### Performance gesture field — synth-wide sound discovery

**PERFORM** introduces two recallable, continuously automatable normalized controls:

- **DRIFT (176)** adds musical LFO depth to otherwise quiet patches, reshapes their existing filter and subharmonic motion and couples its phase to the existing Sound Space/ChronoState routes. DRIFT alone does not add another oscillator or effect.
- **FRACTURE (177)** increases expressive cross-FM and the existing tube/drive excitation. Its interaction with DRIFT reacts to LFO phase, fast/slow state energy, key velocity and pressure rather than being only a static distortion level.

The **lower-left XY position is neutral** (0, 0). It intentionally preserves the old sound for every legacy patch or session. The field smooths changes on the audio thread over approximately 12 ms, so a continuous drag can be used as a performance gesture instead of editing a pile of deep controls. The two axes are ordinary VST3 parameters that can be automated separately; configurable controller knobs sending **CC20 and CC21** are also supported through the VST3 MIDI controller mapping. CC20/21 are **opt-in assignments**, not a claim that all Arturia controllers transmit those numbers by default. REAPER MIDI Learn remains another supported route.

The three recently added creative factory patches use distinct moderate gesture positions: TUBE FOUNDRY (0.30, 0.42), CLOCKWORK SUB (0.78, 0.55) and GLASS CHAMBER (0.66, 0.16). Other existing sounds remain neutral by default. Existing tube, oscillator, filter, EQ, ChronoState and Sound Space editing remains available in DEEP, including complete bypass and independently recallable settings.

**Mono-key priority repair:** In monophonic legato, press and hold C, add E then G, and release G; the sound should return to E. Release E; it should return to C. Releasing lower keys must not release a different top key. Pedal-up and all-notes-off still release appropriately. This is a correction to the original voice manager, not a new keyboard layer.

This wave's focused local C++ check exercised the **exact** header used by the product (`include/monkeys_ear/performance_field.h`) across 200,000 gesture evaluations under normal and address/undefined sanitizers, covering neutral identity, bounded signals, XY coupling and hostile numerical inputs. That is direct proof of the helper implementation, **not** a full native Monkey's Ear build, DAW run or listening-grade audio evidence. New in-product engine and VST3 host regression tests now target whole-synth audible differences, mono held-key return, patch serialization, XY host automation and old-project migration. The previous host probe also contained a duplicate `bend_id` declaration; this was corrected as a source-level build blocker. These larger tests still need actual Windows execution.

### A practical first REAPER playtest

Open the instrument, select **PLAY: Sound Palette → CLOCKWORK SUB**, enable **LFO: Tempo Sync** and **RHYTHM: Follow Host Clock**, then change the DAW BPM between 80, 120, and 160. Try 1/8, 1/16, 1/8-triplet divisions while moving **RHYTHM: Groove Swing** from 0% to 70%; its rhythm can animate the existing filter/FM/sub/drive routes. Switch to GLASS CHAMBER to evaluate slower envelopes and Sound Space instead of tube intensity. For every comparison, listen to the sound **in a mix**, not only in isolation. Mono/key performance, sustain pedal, patch recall, and host automation should remain usable throughout.

For hosts implementing **VST3 IUnitInfo**, the plugin now exposes 15 native groups: PLAY, Oscillators/Voice, Filters/Envelopes, four-band EQ, Rhythm/Modulation, ChronoState, Sound Space, Vocal, the Tube Rack and four nested tubes, and Master/Routing. The native editor adds actual embedded PLAY, PERFORM, KEYS and DEEP pages above the host-native parameter groups. It uses Windows child-window embedding and resizable GDI rendering, so the current editor is **Windows-only**; a host that does not support HWND can still present the normal parameter interface. Host-specific visibility and actual REAPER behavior require execution in REAPER. The new synced factory patches also carry manual fallback BPM, so offline use without transport context remains possible.


### Hardware MIDI keyboard wave — Arturia and other class-compliant controllers

**Playing the instrument uses standard MIDI routed by REAPER**, not a proprietary Arturia plug-in connection. The VST3 instrument declares one 16-channel event input and stereo audio output. Arturia KeyLab/MiniLab families normally offer a regular **MIDI** port for notes and performance, and some models also offer separate **DAW/MCU/HUI** ports for mixer/transport control. Choose the regular MIDI input to play Monkey's Ear. See [Arturia's controller/DAW integration guides](https://support.arturia.com/hc/en-us/articles/18547778386588-MIDI-Controllers-DAW-Integration) for your model and the [REAPER user guide](https://www.reaper.fm/userguide.php).

In REAPER on Windows:
1. Connect the Arturia keyboard over USB, then open **Options > Preferences > Audio > MIDI Devices** and enable its regular MIDI input. Use the separate DAW/MCU port only if you also configure REAPER's control-surface integration; it is not required for notes.
2. Insert **monkeys_ear** as a virtual instrument on a track, **record-arm** it, set the track's input to **MIDI > your Arturia MIDI port > All Channels**, and turn **input monitoring ON**. Select an audible preset and play the keys or pads.
3. On the new **KEYS** editor page, use **Bend Range**, **Velocity Tone**, **Vibrato Depth**, **Mono**, **Legato** and **Glide**. For sustain, attach a pedal that transmits MIDI **CC64**. The plugin also accepts channel pressure (VST3 controller **128**, corrected from the previous wrong 130), and VST3 poly-pressure events on individual notes.
4. Default performance mappings: note number/velocity -> voice, pitch wheel -> pitch bend (controller 129), mod wheel CC1 and CC74 -> Brightness, CC71 -> Bite, CC91 -> Space, CC93 -> Echo, CC64 -> sustain, channel pressure -> the existing aftertouch performance control. REAPER's own MIDI Learn can associate your knobs and faders with other exposed parameters without requiring a custom script.
5. If the keys do not sound, first check that **the correct MIDI port is enabled**, that the track is armed and monitoring, and that you inserted the **instrument** rather than the FX variant. The external hardware cannot be tested in this source environment.

The note/voice manager now keeps key press identity by MIDI channel and optional VST3 note ID. Same-pitch overlapping notes no longer all release from one key-up, and a held note on channel two is not stopped by a channel-one note-off. Poly pressure applies a bounded harmonic-emphasis change to the matching voice. **This does not claim full MPE support**: pitch bend and channel pressure remain global performance controls, and the MIDI sustain mapping remains global. Model-specific DAW transport buttons and Arturia-specific display/feedback functions are outside this generic VST3 instrument protocol.

This wave includes a deterministic native voice test plus a Windows VST3 host probe for the corrected MIDI controller assignments, per-note pressure delivery, and the **KEYS** editor automation path. All native/host build and actual REAPER/audio behavior remain **unverified until executed on the target machine**.

### Native editor — first physical-test checklist

1. Open `monkeys_ear.vst3` as a new instrument in REAPER on Windows. Confirm the **MONKEY'S EAR** editor appears in the ordinary plug-in window; there must be no separately launched standalone demo.
2. Click **CLOCKWORK SUB** on PLAY, then verify the macros, rhythm values, and tube controls display that patch's current values. Switch to **GLASS CHAMBER** and confirm the editor updates again.
3. Drag **Brightness** while recording automation. Move it from REAPER's automation lane afterward and verify the editor follows the host; undo/redo must not leave stale controls. All GUI writes go through the VST3 component handler.
4. Try attack, sustain, release, division, DAW tempo sync and groove without opening EXPLORE. Then search EXPLORE for `tube`, `filter`, `sustain`, and `space`. Make sure no parameter is stranded behind the PLAY layer.
5. Resize the native window, close it, reopen it, switch instances, and recall a project containing different patches in each instance. Verify one editor never changes the other instrument.
6. Move the host between 32/64/128 audio buffers while dragging controls. The editor must not introduce locks, allocation or rendering on the audio thread.
7. Evaluate with actual human playtime: can you understand the first screen without a manual, and can you deliberately reach the strange mechanisms when you want them?

This wave adds a Windows-native VST3 `IPlugView`, a host automation-edit bridge, and shared factory-preset parameter projection for controller/processor agreement. The new Windows host probe covers editor attachment, edit callbacks, preset refresh, resize, detach, and reattach. These checks are committed source **until actually run against the rebuilt plug-in**; they do not certify REAPER or subjective usability.

### Host reliability and build verification wave

This source wave reinforces the **existing synth and effects**, not the feature count:

- **Reliable tempo-follow requests:** both VST3 instrument and FX components implement `IProcessContextRequirements` (Steinberg VST 3.7+) and explicitly request host tempo, musical quarter-note position, and transport state. The LFO keeps the saved manual BPM fallback when the host omits the corresponding validity flags. Host-delivered musical timing is a requirement of the interface, but an actual REAPER session must still confirm the host provides and updates it.
- **Preserved long tails:** the combined VST3 reports a conservative **26-second stereo effect tail** at the current host sample rate instead of a hardcoded 48,000 samples (~one second). The upper bound accounts for the original feedback delay (up to ~0.7 s at 75% feedback) and FDN reverb (up to 6.5 s T60), including diffusion. The estimate is intentionally patch-independent so selecting a longer patch does not require unreliable plugin-to-host restart signaling. This can make tail-sensitive offline render workflows longer; it prioritizes preserving sound.
- **Correct output signaling:** each rendered stereo buffer clears VST3 output silence flags. The synth's voices, delays, reverb and state can remain audible after input notes stop, so it is not safe to infer output silence from no incoming MIDI or an audio input's silence flags.
- **Idle GUI efficiency:** editor parameter values are polled on the UI thread, but GDI redraw happens only after a control actually changes or the interface is otherwise invalidated. This avoids continuous full-window repaints just to check for REAPER automation.
- **Build portability:** `build.bat` and `run_tests.bat` now locate the current user's existing WinGet WinLibs installation by `%LOCALAPPDATA%`, accept an explicit `TOOLCHAIN_BIN`, or fall back to `g++.exe` already on `PATH`. Ninja is still required. Tests still rebuild from current source; they are not replaced with GitHub CI.

The Windows VST3 host regression now checks transport-context interface discovery, the three required bits, 48/96 kHz tail scaling and clearing artificially pre-marked output silence flags. **These are new regression assertions in the committed source; they have not yet been executed in an actual rebuilt Windows plugin or REAPER.** CMake, Win32 graphics embedding, real audio-thread performance, a physical Arturia performance, and level-matched listening remain release gates. This wave does not claim a completed Windows build.

### Critique and remaining acceptance gates

DRIFT/FRACTURE is a **playable coupling of existing synthesis mechanisms**; its novelty is in the relationships and expressive interaction, not a claim of unprecedented audio algorithms. The field is deliberately bounded, and neutral bypass is exact, but high FRACTURE can increase cross-modulation aliasing and perceived loudness: compare at matched level, 44.1/48/96 kHz, and in an actual mix. Real approval requires blind listening and the musician's judgement, not solely a different waveform or green test. A two-axis gesture also does not replace future per-note MPE or a graphical, editable modulation routing canvas.



These integrations answer basic timing, expressivity and discoverability concerns; they do **not** prove the sound is innovative or the workflow is excellent. A rhythmic phase warp is a creative control rather than a new synthesis architecture. A native PLAY/EXPLORE editor now exists in source, but it still needs real REAPER proof, accessibility and high-DPI review, and musician usability feedback. There is no per-step sequencer or performance-snapshot workflow yet, and no measured REAPER audio deadline or perceptual listening evidence for this editor wave. DAW-beat synchronization is block-context-based; hosts may not provide valid musical position, in which case the LFO free-runs at the clock rate. No lookahead or new reported latency has been introduced.

Advanced sound design: `RACK: Enable` (124), `Parallel Pairs` (125), `Shared Supply` (126), `Output` (127), `Anti-alias` (128), plus four sets of eight controls: `TUBE N: Enable / Tube Type / Drive / Bias / Plate Load / Cathode Sag / Coupling / Mix`. Every slot can independently select **14 tube-inspired profiles**: 12AX7, 12AT7, 12AY7, 12AU7, 5751, 12DW7, 6SN7, 6DJ8, EF86, 6V6, 6L6GC, EL84, EL34 and KT88. These are creative operating-point signatures, **not measured hardware equivalence**. Serial and paired-parallel routing use the same rack in the product signal path. Disabling the rack selects the original `TubeDriveStage` verbatim; old presets default off and use the old DSP.

This source wave has a focused `monkeys_ear_tube_rack_probe` alongside the original native/host probes. **All results from earlier versions' timing tables must be remeasured** with the four-tube DSP enabled; those historical numbers are not evidence of this wave's REAPER performance. Old host state snapshots (124/172/174 parameters) and old text presets remain loadable. The current host snapshot is 178 parameters; older 176-parameter sessions migrate with both new gesture axes set to zero, and CC64 is never persisted as pressed. The three new sounds are ideas to explore, **not algorithmic claims of optimal tone**.

## Tonal Motion Architecture

The main synth and FX wrapper now expose **178 VST3 parameters** (including two hidden performance controls). Existing IDs `0..123` remain stable. The appended `124..177` controls add a four-position tube rack, both existing voice/filter ADSRs, and musical-tempo subdivision of the existing LFO. Tube modeling is **one optional part** of the instrument, not a new product or an automatic change to old sounds. Three new built-in factory patches demonstrate interactions with oscillators, subharmonics, ChronoState, filters, Sound Space, and rhythm.

### Bounded Sound Space

`SoundSpaceProcessor` keeps the low-frequency weight path independent and applies character motion through bounded mechanisms:

- Pitch field: `f' = f * 2^(c/1200)`, with stateful cents displacement bounded by the musician's pitch limit.
- Harmonic field: six independently moving partials use frequency-dependent freedom, keeping lower partials narrower than upper character.
- Modal field: four second-order resonators are driven by character with independently bounded detuning.
- Spectral negotiation: fast/slow weight energy drives a bounded, slewed character low-band yield that returns toward neutral.
- Phase: a bounded mono-safe all-pass relationship shapes character phase before weight recombination.

Routes are data (`source -> destination -> depth`), so LFO, envelope, state energy, direction, aftertouch, velocity, and audio energy can be rewired without a new DSP feature.

The weight path supplies independent fundamental and 1/1, 1/2, 1/3, or 1/4 subharmonic energy with phase, polarity, envelope-follow, and saturation controls. MIDI is oscillator locked. The FX path uses a causal positive-crossing tracker for stable monophonic input from 45–500 Hz. It needs two crossings (about 4–44 ms across that range), adds no hidden lookahead or reported plugin latency, and fades its generated sub when pitch confidence falls. Chords, noisy material, weak fundamentals, and rapid transitions are deliberately treated as uncertain rather than advertised as perfect tracking.

ChronoState remains a real state engine in the character path. Its fast energy, slow energy, and signed direction can drive FM depth, weight/character balance, resonator scale, and filter movement. Conventional LFO, envelope, velocity, key tracking, MIDI controls, and aftertouch-depth controls remain independently predictable.

The limiter is a bounded signal-protection stage. It is not a hearing-safety guarantee and does not replace safe monitor gain.

---

## Build & Run

### Quick Build (CMake + Ninja + GCC 14.2)
```powershell
.\build.bat
```

### Run Tests, Offline Audio Renderer, and Latency Benchmark
```powershell
.\run_tests.bat
```

### Targets Produced
- `build\libmonkeys_ear_core.a` — Static C++20 DSP Core Engine
- `build\monkeys_ear_test_runner.exe` — Deterministic Test Suite & Offline Audio Renderer
- `build\monkeys_ear.vst3` — Native VST3 Plugin Bundle for REAPER / DAWs
- `build\monkeys_ear_vocal.vst3` — Standalone Vocal effect: local causal pitch tracking and the established dual-layer Vocal mechanism only
- Installed to `C:\Program Files\Common Files\VST3\monkeys_ear.vst3`

Voice/Vocal has its own pre-release identity and changelog, independent of the
unfinished suite. See [`docs/VOICE_VOCAL_CHANGELOG.md`](docs/VOICE_VOCAL_CHANGELOG.md).
The current Itch-ready copy draft lives at [`docs/VOICE_VOCAL_STOREFRONT_COPY.md`](docs/VOICE_VOCAL_STOREFRONT_COPY.md). It is a productization artifact, not evidence that the preview has cleared its remaining host/listening/release gates.
