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
       └─────┬──────┘ (Linear crossfade: Macro 7 Mic Blend)
             ▼
   ┌───────────────────┐
   │ StateVariableFilt │ (Cytomic ZDF SVF, 2-pole resonant, nonlinear feedback saturation)
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
