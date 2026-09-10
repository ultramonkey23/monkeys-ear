# ARCHITECTURE.md — Monkey's Ear Technical Architecture Spine

## 1. Vertical Audio Pipeline

```
[MIDI In]      [Live Mic In]
   │                │
   │           ┌────┴────────────┐
   │           │ AudioInputProc  │ (Preamp, 20Hz DC-block, 8192-sample ring buffer)
   │           └────┬────────────┘
   ▼                │
┌──────────────┐    │
│ VoiceManager │    │ (16-voice polyphony, antialiased PolyBLEP Saw/Pulse/Tri/Sine, ADSR)
└──────┬───────┘    │
       │            │
       └─────┬──────┘ (Instrument/FX/hybrid source routing)
             ▼
   ┌───────────────────┐
   │ Weight / Character │ (pitch-locked MIDI weight; confidence-gated FX divider)
   └──────┬──────┬─────┘
          │      └──────── clean low anchor (fundamental + selected 1:n sub)
          ▼
   ┌───────────────────┐
   │  ChronoStateBody  │ (character state + fast/slow/direction observations)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │ MultiPassFilter   │ (two reusable ZDF passes, five modes, 12/24 dB, three routes)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │  TubeDriveStage   │ (Asymmetric triode waveshaper, dynamic cathode sag memory)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │ CabinetResonator  │ (4-pole modal chassis resonator, acoustic cavity modeling)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │ Weight Recombine  │ (bounded state/LFO balance; clean sub bypasses character drive)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │ 4-Band Param EQ   │ (bell/shelves/HP/LP; smoothed coefficients; gain compare)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │    StereoDelay    │ (Ping-pong cross feedback, 4-point Hermite cubic interpolation)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │     FDNReverb     │ (8-line Feedback Delay Network, orthogonal Hadamard matrix, air damping)
   └─────────┬─────────┘
             ▼
   ┌───────────────────┐
   │   SafetyLimiter   │ (Soft-knee peak compression, brickwall ceiling, denormal scrub)
   └─────────┬─────────┘
             ▼
     [Stereo Audio Out]
```

## 2. Hard Real-Time Guarantees
- **Allocations**: Zero dynamic allocations in `process_block()`. All delay lines, voice pools, buffers, and tables are allocated during `init()`.
- **Concurrency**: Zero locks, mutexes, condition variables, or atomic spins in audio thread.
- **I/O**: Zero file, network, or console printing operations during audio callbacks.
- **Latency**: Exactly 0 samples reported plugin latency.

## 3. Host Integration
- VST3 Component & Processor COM implementation adhering to Steinberg ABI.
- Class ID: `4D6F6E6B-6579-7345-6172-496E73747201`.
- Targets: REAPER on Windows x64.
- Dual classes remain: pure Instrument (no audio input) and FX (stereo input).
- 111 musician-facing automated parameters use functional name prefixes as host-visible groups; two hidden VST3 performance inputs carry hardware pitch bend and channel pressure without cluttering the control surface. IDs 0-81 remain stable; Sound Space IDs 82-112 are appended.
- Component/controller state is serialized as a versioned fixed-size stream; no audio-thread allocation is used for parameter application.
- Factory target patches are selectable by the `PRESET: Target Patch` parameter and are also rendered by the deterministic test runner as `.mepreset` + `.wav` pairs.

## 4. Motion Contract

The bounded source-transform-destination layer is intentionally small. Sources currently used are LFO, amp/filter envelope, velocity, key tracking, aftertouch value, external audio envelope, ChronoState fast energy, slow energy, and signed direction. Destinations currently used are dual-pass cutoff/resonance, Cross-FM depth, sub balance, tube drive, resonator scale, EQ band-2 frequency/gain, and performance vibrato. Curves and dedicated one-pole smoothers keep destination changes causal and bounded.

External pitch tracking is not polyphonic pitch detection. It accepts stable monophonic 45–500 Hz input, requires two positive crossings, and confidence-fades on invalid or stale periods. It reports zero host latency because it uses no lookahead; the musical acquisition delay is signal-dependent and explicitly not hidden.

## 6. Vocal Pitch + Expression Evolution Surface

`AudioInputProcessor` owns the live vocal path. It separates an estimated periodic component from a retained aperiodic component with a causal confidence/energy blend, so it can represent **periodic**, **mixed**, or **aperiodic** material instead of forcing a binary verdict. The primary renderer is trailing-grain PSOLA. An optional, fixed-history second PSOLA pass shares correction across two softer transformations; it is not a disguised hard renderer switch. A separately capped high-frequency residual and one broad envelope repair remain controllable. All stages have fixed memory, use no future sample, allocations, locks, I/O, or network, and their outcomes are currently synthetic/code evidence—not listening or release evidence.

## 5. Bounded Sound Space Contract

`SoundSpaceProcessor` is a reusable source-transform-destination layer after the character/body path and before the production EQ. It has no allocations or locks in `process_block()`.

The pitch field uses `f' = f * 2^(c/1200)` with a stateful bounded cents displacement. Six additive upper partials and four modal resonators have independent bounded fields; allowable deviation widens by frequency only through the user-controlled freedom curve and energy widening. The fundamental and selected subharmonics remain in the weight path.

Weight fast/slow energy is observed in separate one-pole histories. A priority-weighted detector negotiates a bounded low-band character gain, slewed by resistance and attracted back to neutral. Character phase uses a bounded first-order all-pass relationship; the weight anchor bypasses it, preserving mono low-frequency solidity. Four routes expose `source -> destination -> depth` for LFO, envelope, fast/slow state energy, direction, aftertouch, velocity, and external audio energy.
