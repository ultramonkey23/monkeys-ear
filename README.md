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

In REAPER's standard VST parameter controls (there is no custom graphical editor yet), use **PLAY: Sound Palette** (ID 173) to audition:
- `0 Current`, `1 MONOLITH`, `2 FERAL WOBBLE`, `3 VELVET LEAD` (unchanged legacy choices).
- `4 TUBE FOUNDRY` — four serial tube positions, modulation and cathode/supply interaction.
- `5 CLOCKWORK SUB` — two parallel tube pairs, subharmonics and tempo-divided movement.
- `6 GLASS CHAMBER` — long expressive envelopes, modal Sound Space and subdued nonlinear coloration.
- `7 SOUND SPACE` — original Sound Space factory demonstration.

The earlier **PRESET: Target Patch** control (ID 79) deliberately stays a four-option legacy selector: Current, MONOLITH, FERAL WOBBLE and VELVET LEAD. This preserves old project and automation meanings. **PLAY: Sustain Pedal** (ID 172) supports MIDI CC64, with held notes releasing only when the pedal rises. Sustaining multiple repeated notes remains polyphonic; host deactivate resets the voices. Host MIDI note-on/off timing and parameter automation are now scheduled by sample offset within each audio block. The host accepts no-audio parameter flush calls. The sustained pedal position is an ephemeral performance input and is never reopened as down on session restore.

For immediate playability, start with the existing eight macros **Brightness / Bite / Heat / Body / Echo / Space / Mic Blend / Character**, then adjust `AMP: Attack/Decay/Sustain/Release` (IDs 161..164), `FILTER ENV: Attack/Decay/Sustain/Release` (165..168), `LFO: Tempo Sync` (169), `LFO: Tempo BPM` (170) and `LFO: Subdivision` (171). Sync is **manual BPM**, not yet DAW transport-following: a recalled patch continues to play deterministically without host-tempo data. The 12 divisions are 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, quarter/ eighth triplets and dotted eighth/sixteenth.

Advanced sound design: `RACK: Enable` (124), `Parallel Pairs` (125), `Shared Supply` (126), `Output` (127), `Anti-alias` (128), plus four sets of eight controls: `TUBE N: Enable / Tube Type / Drive / Bias / Plate Load / Cathode Sag / Coupling / Mix`. Every slot can independently select **14 tube-inspired profiles**: 12AX7, 12AT7, 12AY7, 12AU7, 5751, 12DW7, 6SN7, 6DJ8, EF86, 6V6, 6L6GC, EL84, EL34 and KT88. These are creative operating-point signatures, **not measured hardware equivalence**. Serial and paired-parallel routing use the same rack in the product signal path. Disabling the rack selects the original `TubeDriveStage` verbatim; old presets default off and use the old DSP.

This source wave has a focused `monkeys_ear_tube_rack_probe` alongside the original native/host probes. **All results from earlier versions' timing tables must be remeasured** with the four-tube DSP enabled; those historical numbers are not evidence of this wave's REAPER performance. Old host state snapshots (124/172 parameters) and old text presets remain loadable. The current host snapshot is 174 parameters; CC64 is never persisted as pressed. The three new sounds are ideas to explore, **not algorithmic claims of optimal tone**.

## Tonal Motion Architecture

The main synth and FX wrapper now expose **174 VST3 parameters** (including two hidden performance controls). Existing IDs `0..123` remain stable. The appended `124..173` controls add a four-position tube rack, both existing voice/filter ADSRs, and musical-tempo subdivision of the existing LFO. Tube modeling is **one optional part** of the instrument, not a new product or an automatic change to old sounds. Three new built-in factory patches demonstrate interactions with oscillators, subharmonics, ChronoState, filters, Sound Space, and rhythm.

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
