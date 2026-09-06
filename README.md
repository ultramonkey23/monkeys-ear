# Monkey's Ear (ME-01)

> **Production-grade low-latency digital instrument and sound-design environment.**
> Combining synthesis, sampling, live audio/vocal processing, physical/nonlinear modeling, modulation, amplification, delay, reverb, and semantic control.

---

## First Playable Vertical Core

```
MIDI / MIC / LIVE AUDIO
           ↓
        SOURCE (Dual PolyBLEP Antialiased Osc + Sub + Noise)
           ↓
        FILTER (Zero-Delay Feedback State Variable Filter with Nonlinear Saturation)
           ↓
     DRIVE / TUBE (Causal Asymmetric Waveshaping with Dynamic Cathode/Sag Memory)
           ↓
    CAB / RESONATOR (4-Pole Acoustic Body / Modal Chassis Resonator)
           ↓
         DELAY (Stereo Cross-Feedback Ping-Pong with 4-point Hermite Interpolation)
           ↓
   FDN / ALGORITHMIC REVERB (8-Line Feedback Delay Network with Orthogonal Hadamard Matrix)
           ↓
  OUTPUT & SAFETY LIMITER (Zero-Latency Soft-Knee Brickwall Ear Protection & Denormal Scrubbing)
```

---

## Real-Time Performance & Proof

Verified on Windows x64 @ 48kHz (Build with CMake + Ninja + GCC 14.2):

| Buffer Size | Block Deadline | Measured Avg Block Time | Measured Max Block Time | Deadline Margin | Deadline Misses |
|---|---|---|---|---|---|
| **32 samples** | 666.67 $\mu s$ | **5.81 $\mu s$** | 8.70 $\mu s$ | **99.13 %** | 0 |
| **64 samples** | 1333.33 $\mu s$ | **54.73 $\mu s$** (full polyphony + FX) | 676.50 $\mu s$ (under peak load) | **95.89 %** | 0 |
| **128 samples** | 2666.67 $\mu s$ | **27.04 $\mu s$** | 48.10 $\mu s$ | **98.99 %** | 0 |
| **256 samples** | 5333.33 $\mu s$ | **45.60 $\mu s$** | 83.20 $\mu s$ | **99.14 %** | 0 |

- **Reported Plugin Latency**: 0 samples
- **Memory Allocations in Audio Path**: 0
- **Locks / Mutexes**: 0
- **Audio Output Verification**: 48kHz 24-bit PCM stereo WAV rendered, peak bounded at -5.51 dBFS, RMS at -17.76 dBFS, zero NaNs/Infs.

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
- Installed to `C:\Program Files\Common Files\VST3\monkeys_ear.vst3`
