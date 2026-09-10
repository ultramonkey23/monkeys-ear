# Monkey's Ear Connected Module Standard

Status: **product invariant**

Monkey's Ear is a connected ecosystem of VST3 effects and instruments. Every release module must be useful when loaded alone in REAPER **and** remain a native participant in the Monkey's Ear ecosystem when other modules are present.

Standalone means **works alone**, not **designed alone**.

## Architecture

`host inputs -> module-local fallback analysis -> shared Monkey's Ear performance/state language -> module DSP -> host output`

When compatible Monkey's Ear modules are connected, they may exchange bounded typed musical/performance evidence through the shared ecosystem layer. When no ecosystem peer is available, each plugin falls back locally and remains fully usable.

The ecosystem is not a mandatory monolithic suite and it is not a collection of isolated plugins. Individual VST3 identities are first-class products; connection enriches them.

### Shared language, bounded communication

Shared evidence is typed and carries enough context to be interpreted safely: value, confidence, age, provenance/source, temporal horizon and relevant musical/band context. Consumers decide whether evidence is authoritative enough to use.

Cross-module interaction is explicit and bounded rather than unrestricted all-to-all modulation. The established interaction vocabulary is:

`SOURCE OPERATOR DESTINATION`

with operators such as `ATTRACT`, `RESIST`, `REPEL`, `DISSIPATE`, `INJECT`, and `COUPLE`.

Automatic actions are opt-in, bounded, recallable and reversible. The audio thread never waits for ecosystem communication. Missing, stale or invalid shared evidence degrades to local behavior, never silence or invalid DSP.

Do not create duplicate per-plugin routers, registries, detector universes or learning ledgers. Shared infrastructure has one canonical contract; module-local analysis exists as a fallback/capability owner, not as a competing architecture.

## Module ownership

Each VST3 module owns:

- a stable host-facing identity and unique component/controller IDs;
- an explicit audio/MIDI I/O contract;
- safe, musically useful defaults;
- module-scoped versioned state and transactional recall;
- bypass and lifecycle/reset behavior;
- automation-safe parameter transitions;
- finite-output/fallback behavior;
- the local capabilities required to remain useful when no ecosystem peer exists;
- adapters to publish/consume compatible shared Monkey's Ear evidence;
- its own build artifact and host proof.

A module must never require another plugin instance merely to pass audio or produce its core advertised behavior. Conversely, it must not throw away useful shared evidence merely because it can operate alone.

## Ease-of-use hierarchy

Complexity is layered, not removed.

### PLAY
The first visible surface answers **what do I turn to make this useful?** A small set of primary musical controls must work without documentation or ecosystem setup.

### ADVANCED
Mechanism controls with a meaningful audible consequence.

### LAB
Deep, strange and difficult controls are welcome when they expose a real sonic mechanism. They remain bounded, recallable and automation-safe, and are never required for ordinary operation.

Ecosystem connection follows the same rule: useful automatic/default behavior first; explicit routing and deep interaction controls remain available underneath.

## REAPER engineering gate

A module is not ready until an actual artifact proves:

1. REAPER discovers and instantiates it under its own identity.
2. Declared audio/MIDI buses match its purpose.
3. Silence and normal signal remain finite.
4. Core behavior works with no other Monkey's Ear module loaded.
5. Bypass is safe and appropriately transparent.
6. Aggressive parameter automation cannot create invalid state or unsafe discontinuities.
7. Module state round-trips and malformed state cannot partially mutate live state.
8. Sample-rate and buffer-size changes remain correct.
9. Offline render does not depend on UI state or live ecosystem peers.
10. Connected mode tolerates absent, stale, reordered or invalid shared evidence and falls back locally.
11. Connection/disconnection cannot block the realtime thread.
12. Shared evidence has provenance/confidence/age and cannot silently become an unbounded control signal.

## REAPER product gate

Engineering proof is necessary but insufficient. Human proof before release includes project close/reopen recall, immediate usefulness of primary controls, useful defaults, unobstructed PLAY workflow, and listening evaluation. Connected workflows must feel like an enhancement rather than setup tax.

Tests and agents must never manufacture subjective usability or listening proof.

## State rule

State is module-scoped but ecosystem-aware. Vocal recalls Vocal without needing a Synth preset. If a preset contains ecosystem links/interactions, those links are versioned and validated separately from the module's core state. Missing peers leave the module's valid local state intact.

State loading is transactional: parse and validate a candidate, then commit it. Malformed, truncated, non-finite or unsupported state leaves the last valid live state unchanged.

## Development workflow

For every module:

`ecosystem contract -> module/local fallback capability -> deterministic tests -> VST3 wrapper -> standalone host proof -> connected-mode proof -> human REAPER/listening proof`

Do not build only inside a monolith and promise extraction later. Do not isolate a module so aggressively that it cannot participate in the ecosystem later.

## Current migration rule

The existing `monkeys_ear` wrapper is legacy integration while individual VST3 identities mature. Existing DSP remains canonical. `VocalModule` is a boundary around that DSP, not a new vocal engine. Its current explicit pitch-evidence input should evolve into a shared-analysis consumer with a local causal fallback, rather than a second detector architecture that competes with the ecosystem.
