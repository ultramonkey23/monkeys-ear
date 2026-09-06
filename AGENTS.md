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
- Absolute speaker and hearing safety: Every output path passes through a zero-latency brickwall safety limiter and denormal/NaN scrubber.

---

## 3. MASTER CONVERGENCE & LAB BOUNDARIES

- Code Prime remains the sole intentional mutation boundary.
- Any temporary branches are disposable transport; **master convergence is mandatory**.
- Do not duplicate Lab organs or create parallel authority structures inside Monkey's Ear.
- Proof prevents lies; proof does not choose the dream. Cody remains the final musical authority.
