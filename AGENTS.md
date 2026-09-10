# AGENTS.md — Monkey's Ear Repository Contract

> **CANONICAL ENTRY CONTRACT.** This repository is the native C++ instrument and sound environment body for **Monkey's Ear**, registered with and orchestrated by **Ultramonkeydog Lab**. The Lab root constitution lives at [`C:\Users\harin\ultramonkeydog-lab\AGENTS.md`](file:///C:/Users/harin/ultramonkeydog-lab/AGENTS.md).

---

## 1. PRODUCT IDENTITY & ANTI-TUNNEL-VISION LAW

Monkey's Ear is **NOT** a latency, ML, chronofrequency, rollback-netcode, physical-modeling, semantic-AI, or modular-synth research project.

The product is:
> **A connected ecosystem of production-grade VST3 effects and instruments: fast enough to perform through, deep enough to produce with, controllable enough for deliberate music making, weird enough to discover genuinely new sound, useful alone in a host such as REAPER, and richer when Monkey's Ear modules are connected together.**

No subsystem may redefine the product around itself.

---

## 2. HARD REAL-TIME AUDIO CONTRACT

LIVE is sacred:
- **0 samples reported plugin latency** to host where the module's established LIVE design permits it.
- **Strict zero allocation / zero deallocation** on the audio thread during `process()`.
- **Zero locks, zero mutexes, zero file I/O, zero network calls, zero UI operations** in the real-time path.
- Hard deadline margin target: >90% at 32 / 64 / 128 sample buffers.
- Signal-level protection: output paths require denormal/NaN protection and the established zero-latency safety strategy. Monitoring level remains the musician's responsibility.
- Ecosystem communication is never allowed to make the audio thread wait.

---

## 3. CONNECTED VST3 ECOSYSTEM & EASE-OF-USE LAW

**Standalone means works alone, not designed alone.**

Every release module has its own useful VST3 identity in REAPER and also remains a native Monkey's Ear ecosystem participant. A module must provide its core advertised behavior when no peer exists, while being able to publish/consume compatible shared musical and performance evidence when peers are connected.

Canonical direction:
> `host inputs -> local fallback capability -> shared Monkey's Ear performance/state language -> module DSP -> host output`

Shared analysis/state is typed and contextual: value, confidence, age, provenance, temporal horizon and relevant musical/band context. Cross-module behavior is explicit and bounded, using the established `SOURCE OPERATOR DESTINATION` model with operators such as `ATTRACT`, `RESIST`, `REPEL`, `DISSIPATE`, `INJECT`, and `COUPLE`. Shared state is **not unrestricted all-to-all modulation**.

Missing/stale/invalid peers or evidence degrade to local behavior. Automatic ecosystem actions are opt-in, bounded, recallable and reversible. Do not create duplicate per-plugin routers, registries, detector universes, ledgers or authority systems merely to achieve standalone operation.

Ease of use is layered:
- **PLAY** — few immediate musical controls; useful without documentation or ecosystem setup.
- **ADVANCED** — meaningful mechanism controls.
- **LAB** — deep/weird/hard-to-understand controls are welcome when they expose a real sonic mechanism, but remain bounded and are never required for ordinary use.

Each module owns stable identity, truthful audio/MIDI I/O, safe/useful defaults, module-scoped transactional state, bypass, automation-safe transitions, lifecycle/reset, local fallback capabilities, ecosystem adapters, finite failure behavior, and artifact-local host proof.

A module is not release-ready until actual REAPER proof covers standalone and connected behavior, followed by human project-recall, ease-of-use and listening proof. Automated evidence must not claim subjective usability or sound quality.

See `docs/STANDALONE_MODULE_STANDARD.md` (historical filename; content is the connected-module standard) and `include/monkeys_ear/module_contract.h`.

---

## 4. MASTER CONVERGENCE & LAB BOUNDARIES

- Code Prime remains the sole intentional mutation boundary.
- Any temporary branches are disposable transport; **master convergence is mandatory**.
- `master` is the canonical production line. Compatibility refs such as `main` fast-forward after a coherent production wave; no silent long-lived divergence.
- Do not duplicate Lab organs or create parallel authority structures inside Monkey's Ear.
- Proof prevents lies; proof does not choose the dream. Cody remains the final musical authority.
