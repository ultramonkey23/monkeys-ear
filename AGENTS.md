# AGENTS.md — Monkey's Ear Repository Contract

> **CANONICAL ENTRY CONTRACT.** This repository is the native C++ instrument and sound environment body for **Monkey's Ear**, registered with and orchestrated by **Ultramonkeydog Lab**. The Lab root constitution lives at [`C:\Users\harin\ultramonkeydog-lab\AGENTS.md`](file:///C:/Users/harin/ultramonkeydog-lab/AGENTS.md).

---

## 1. PRODUCT IDENTITY & ANTI-TUNNEL-VISION LAW

Monkey's Ear is **NOT**:
- a latency research project;
- an ML project;
- a chronofrequency project;
- a rollback-netcode experiment;
- a physical-modeling project;
- a semantic-AI project;
- a modular synth research toy.

The product is:
> **A production-grade live instrument and sound environment that is fast enough to perform through, deep enough to produce with, controllable enough for deliberate music making, weird enough to discover genuinely new sound, and structurally strong enough to keep growing.**

No subsystem may redefine the product around itself.

---

## 2. HARD REAL-TIME AUDIO CONTRACT

LIVE is sacred:
- **0 samples reported plugin latency** to host.
- **Strict zero allocation / zero deallocation** on the audio thread during `process()`.
- **Zero locks, zero mutexes, zero file I/O, zero network calls, zero UI operations** in the real-time path.
- Hard deadline margin target: >90% at 32 / 64 / 128 sample buffers.
- Signal-level protection: Every output path passes through a zero-latency limiter and denormal/NaN scrubber. This does not guarantee hearing safety; monitoring level remains the musician's responsibility.

---

## 3. STANDALONE MODULE & EASE-OF-USE LAW

Every release module must be a useful independent plugin in REAPER. Shared DSP is encouraged; requiring another Monkey's Ear plugin instance is forbidden.

Architecture converges toward:
> `shared DSP primitives -> module processor -> standalone VST3 wrapper -> optional suite/composite host`

Each module owns a stable host identity, explicit audio/MIDI contract, safe/useful defaults, module-scoped versioned state, bypass, automation-safe transitions, lifecycle/reset behavior, finite fallback, and artifact-local host proof. State loading is transactional: malformed state must not partially mutate the last valid state.

Ease of use is layered rather than simplified away:
- **PLAY** — the few primary musical controls; immediately useful without documentation.
- **ADVANCED** — meaningful mechanism controls.
- **LAB** — deep/weird controls are welcome when they expose a real sonic mechanism, but remain bounded and are never required for ordinary use.

A module is not release-ready until an actual REAPER artifact proves instantiate/I-O/silence/signal/bypass/automation/state/sample-rate/buffer/offline-render behavior, followed by human project-recall and usability/listening proof. Automated evidence must not claim subjective usability or sound quality.

See `docs/STANDALONE_MODULE_STANDARD.md` and `include/monkeys_ear/module_contract.h`.

---

## 4. MASTER CONVERGENCE & LAB BOUNDARIES

- Code Prime remains the sole intentional mutation boundary.
- Any temporary branches are disposable transport; **master convergence is mandatory**.
- `master` is the canonical production line. If compatibility refs such as `main` exist, fast-forward them after a validated production wave; do not allow silent long-lived divergence.
- Do not duplicate Lab organs or create parallel authority structures inside Monkey's Ear.
- Proof prevents lies; proof does not choose the dream. Cody remains the final musical authority.
