# Monkey's Ear — product-wide research spine

Status: durable research guidance only. The GitHub repository is not yet the complete Monkey's Ear product source; a more complete local implementation predates this repository and must be imported/audited before product-integration decisions.

## Purpose

This file prevents subsystem research from drifting into disconnected inventions. Each Monkey's Ear part may have its own focused research folder, but all work must obey the same product laws, evidence workflow, real-time constraints, and integration boundary.

## Product thesis

Monkey's Ear is not one plugin or one effect. It is a programmable audio system spanning synthesis, sampling, live manipulation, pitch control, EQ/dynamics, nonlinear/physical modeling, modulation, amplification, spatial processing, profiling/capture, sequencing and routing.

The product should expose mechanisms rather than hard-coded sounds:

> Do not implement a sound when you can implement the controllable mechanism that creates it. Presets demonstrate mechanisms; they do not define their limits.

And every unusual mechanism must obey:

> Anything weird must be optional. Anything useful must be controllable. Anything used in a song must be recallable.

## Shared architecture laws

1. **Normal operation remains excellent.** Experimental behavior may never make basic EQ, pitch correction, synthesis, routing or dynamics worse.
2. **Evidence, intent and action stay separate.** Measurement does not authorize correction by itself.
3. **Candidate actions are bounded.** Where practical, proposed corrective actions should be evaluated before commitment rather than blindly following detectors.
4. **Everything is automatable, recallable, bypassable and reversible.**
5. **No mandatory cloud inference.** Local deterministic DSP remains authoritative; optional ML may assist outside hard real-time paths.
6. **No hidden lookahead.** Any future information requires explicit buffering/latency or offline pre-analysis.
7. **No monolithic smart engine.** Shared primitives may be reused across subsystems, but evidence models, candidate generation and actions remain replaceable.
8. **Do not duplicate Lab authority.** Code Prime remains the intentional mutation boundary; audio specialists are capabilities, not a parallel command structure.

## Signal-chain model

Monkey's Ear should eventually support visible, editable signal chains with serial and parallel paths, split/merge nodes, multiple amps/effects/bodies, independent stems, and separate audio/modulation/state edges. Reusable subgraphs/macros are preferred over hidden routing magic.

Optional source-role concepts such as SUB / BODY / TEXTURE may help organize synthesis and processing, but they are roles, not mandatory architecture.

## Processing modes

Design against three execution contexts rather than pretending one latency/CPU budget fits all:

- **LIVE** — deadline-safe, bounded, low-latency behavior.
- **STUDIO** — more analysis/history and moderate buffering allowed.
- **RENDER** — offline/pre-analysis may use heavier algorithms if results remain reproducible.

Do not claim hardware-specific latency guarantees before physical loopback measurement. Measure average, p95, p99, p99.9 and worst-case callback time, plus misses/xruns, allocations, locks, denormals, plugin delay compensation and round-trip latency.

## Hard real-time rules

No network, disk I/O, decoding, model training, blocking synchronization, unbounded allocation, UI work or uncontrolled inference in the audio callback. Preallocate bounded work. Keep analysis/state transfer timestamped and latency-aligned.

The Lab already treats REAPER as the primary human workbench and favors a framework-independent C++ core with CLAP/iPlug2-style wrappers as candidates, not architectural dependencies. Existing Savage Crown Living Audio Forge work is prior art for deterministic WAV inspection/transforms, provenance, native math-sound rendering and runtime event seams; generalize useful primitives instead of recreating them.

## Cross-subsystem concepts worth preserving

### Microtonal / pitch law

Frequency offset by cents follows `f' = f * 2^(c/1200)`. Independent partial tuning requires a representation that actually exposes partials (for example additive/modal structure); do not fake independent harmonic motion through a representation that cannot support it.

### Phase-aware layering

Useful controls may include oscillator phase/reset, polarity, sub lock, delay, all-pass/frequency-dependent phase, bounded coupling and stereo decorrelation. Validate mono compatibility, cancellation and transient integrity; phase changes are not automatically improvements.

### Capture / profiling

Static impulse responses capture linear time-invariant behavior only. Nonlinear/history-dependent devices require behavioral profiling or other state-aware representations. Store provenance and authorization for every captured source.

### SoundSpace

Treat SoundSpace as an optional configurable source -> operator -> destination graph with bounded strength/timescale/bypass, not as an autonomous generative authority. It must remain inspectable and deterministic when used in production.

### WordMath

Semantic mappings may expose useful high-level control, but semantic interpretation belongs outside hard real-time DSP. Mappings must resolve to inspectable parameter/control changes rather than hidden generative evolution.

## Research workflow for every subsystem

1. **Inspect current truth** — local/imported code, interfaces, docs, tests and known bugs first.
2. **Benchmark current practice** — establish what commercial/open tools already do so novelty is not confused with differentiation.
3. **Define simple baselines** — conventional methods stay in every comparison.
4. **Prototype competing mechanisms** — small, reproducible, independently removable experiments.
5. **Try to break them** — randomized/adversarial cases, ablations and failure logging.
6. **Evaluate real audio** — same source, level matched, identical routing where possible.
7. **Listen blind when practical** — engineering proxies can reject ideas but cannot prove musical preference.
8. **Promote/demote explicitly** — no feature survives because it once sounded clever.
9. **Integrate only after local-source audit** — do not create parallel product implementations from research assumptions.
10. **Consolidate docs** — one active architecture file + one concise research history per subsystem; keep scripts/data for reproducibility, not a stack of versioned design manifestos.

## Current subsystem map

Active research:
- `research/onion-skin-eq/` — EQ / spectral dynamics / relational evidence.
- `research/pitch-correction/` — pitch correction / pitch control.

Planned future research lanes (create only when work actually starts):
- synthesis / voice engine;
- sampling / resampling;
- nonlinear / amplifier / physical-modeling behavior;
- modulation;
- routing / graph execution;
- spatial / phase;
- capture / profiling;
- sequencing / timing;
- SoundSpace;
- semantic control / WordMath.

Do not create empty folders or speculative per-lane design files just to fill this list.

## Durability rule

Complexity is not progress. A mechanism remains active only if it wins a measured engineering tradeoff, wins a controlled listening task, or provides a genuinely distinct user-controlled creative capability. Otherwise simplify, demote, archive the evidence or remove it.
