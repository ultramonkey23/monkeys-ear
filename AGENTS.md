# AGENTS.md — Monkey's Ear Repository Contract

> **CANONICAL ENTRY CONTRACT.** This repository is the native C++ instrument and sound environment body for **Monkey's Ear**, registered with and orchestrated by **Ultramonkeydog Lab**. The Lab root constitution lives at [`C:\Users\harin\ultramonkeydog-lab\AGENTS.md`](file:///C:/Users/harin/ultramonkeydog-lab/AGENTS.md).

---

## 1. PRODUCT IDENTITY & ANTI-TUNNEL-VISION LAW

Monkey's Ear is **NOT** a latency, ML, chronofrequency, rollback-netcode, physical-modeling, semantic-AI, or modular-synth research project.

The product is:
> **A connected ecosystem of production-grade VST3 effects and instruments: fast enough to perform through, deep enough to produce with, controllable enough for deliberate music making, weird enough to discover genuinely new sound, useful alone in a host such as REAPER, and richer when Monkey's Ear modules are connected together.**

No subsystem may redefine the product around itself.

### Product horizon — standalone mobile DAW
Monkey's Ear includes a creator-directed **future standalone Android/mobile DAW** in its product intent, in addition to the existing REAPER-hosted VST3 ecosystem. Mobile is **touch-first**, but not touch-only: it must support external MIDI hardware, physical typing/computer keyboards as playable musical input where supported, and a touch piano roll/sequencers. Its musical workflow combines **patterns, loops, sequencing and live performance**, with deeply controllable quantization. Caustic is a workflow reference, **not a mandate to clone Caustic or a claim that mobile is built**.

**Interoperability intent:** share sounds and compatible presets with Monkey's Ear for REAPER; bridge detailed audio editing through **time-aligned stem export/import**. Do not impose full bidirectional editable project parity as a requirement the creator did not choose. Preset portability must be proved against actual engine/format compatibility; samples, patch metadata and rendered stems are distinct artifacts. The mobile DAW **must work completely without AI**; AI may enhance but cannot own or gate any core musical behavior.

Canonical intent: [`docs/MOBILE_DAW_PRODUCT_VISION.md`](docs/MOBILE_DAW_PRODUCT_VISION.md); engineering/reality-proof boundaries: [`docs/MOBILE_DAW_ENGINEERING.md`](docs/MOBILE_DAW_ENGINEERING.md); external and Lab research intake: [`docs/MOBILE_DAW_RESEARCH_NOTES.md`](docs/MOBILE_DAW_RESEARCH_NOTES.md). This is future product direction, **not** implementation, test, device, or release evidence.

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

- Code Prime remains the sole intentional mutation boundary for Lab-routed product code work. An explicitly authorized direct GitHub maintenance operation must still comply with the Lab's operator-sovereignty and provenance law.
- **Prototyping, experimentation and research-backed trials are parts of full production**, not a separate non-production program. Build and evolve them in the canonical product on `master` under the existing Lab mutation owner, with truthful implementation/proof/release states; an experiment is not automatically ready to ship, but its status is not a reason for a throwaway repo, feature branch, or disconnected implementation. Reuse real capabilities and preserve the production spine; differentiate experiment evidence from accepted user-facing functionality.
- **`master` is the sole canonical development and integration branch.** Never select `main` as a development target, create agent-chosen feature/transport branches or pull requests, or use another branch as a fallback when direct `master` mutation is blocked.
- Existing Code Prime-owned bounded ephemeral validation worktrees are implementation details, not persistent alternate branch authorities; accepted work must converge directly onto `master`, with normal push and remote readback. Do not overwrite concurrent or uncommitted work.
- Any surviving `main` ref is a temporary compatibility alias, never a separate source of truth. It must only fast-forward from validated `master` and remain synchronized until the GitHub default branch is changed to `master` and the alias is safely retired. Until then, explicitly target `master` in all GitHub API/agent operations rather than relying on the repository default.
- The Lab project registry, launchers, and source-resolution owners must agree on canonical `master`; any mismatched or inaccessible owner is a reported blocker, not permission to work on `main`.
- Do not duplicate Lab organs or create parallel authority structures inside Monkey's Ear.
- Proof prevents lies; proof does not choose the dream. Cody remains the final musical authority.
