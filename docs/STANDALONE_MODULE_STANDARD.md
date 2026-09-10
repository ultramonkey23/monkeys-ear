# Monkey's Ear Standalone Module Standard

Status: **product invariant**

Every module intended for release must work as an independent plugin in REAPER. Sharing DSP code is encouraged; depending on another Monkey's Ear plugin instance is forbidden.

## Architecture

`shared DSP primitives -> module processor -> standalone VST3 wrapper -> optional suite/composite host`

The suite is a composition layer, not the owner of module correctness.

Each standalone module owns:

- a stable host-facing identity and unique component/controller IDs;
- an explicit audio/MIDI I/O contract;
- safe defaults that produce a useful starting state;
- module-scoped versioned state and transactional recall;
- bypass behavior;
- automation-safe parameter transitions;
- sample-rate/block-size lifecycle handling;
- finite-output/fallback behavior;
- its own host probe/build artifact.

## Ease-of-use hierarchy

Complexity is not removed. It is layered.

### PLAY
The first visible surface answers: **what do I turn to make this useful?** Keep the small set of primary musical controls here. Loading the plugin must not require opening ADVANCED or LAB.

### ADVANCED
Mechanism controls with a clear musical consequence. These may be technical, but should still correspond to an audible behavior.

### LAB
Deep, strange and experimental controls are welcome when they expose a real sonic mechanism. They remain bounded, recallable and automation-safe. LAB complexity must never be required for ordinary operation.

## REAPER engineering gate

A module is not standalone-ready until an actual artifact can be checked for all of the following:

1. REAPER discovers and instantiates it under its own identity.
2. Its declared audio/MIDI buses match its purpose.
3. Silence remains finite and stable.
4. Normal signal/MIDI produces finite output.
5. Bypass is safe and appropriately transparent.
6. Aggressive parameter automation does not create invalid state or discontinuous unsafe output.
7. Module state round-trips exactly enough to preserve intent.
8. Sample-rate changes are handled without stale state or invalid coefficients.
9. Buffer-size changes do not alter correctness.
10. Offline render works without depending on UI state or another module.

## REAPER product gate

Engineering proof is necessary but insufficient. Before release, a human must additionally prove:

- save project -> close REAPER -> reopen -> module recalls correctly;
- the primary controls are obvious enough to get useful behavior without documentation;
- defaults are musically useful rather than merely numerically safe;
- advanced/LAB controls do not obstruct the primary workflow.

Listening/usability proof is recorded as human evidence. Tests and agents must not claim it automatically.

## State rule

Standalone state is **module scoped**. A Vocal plugin must not need a full synth preset to recall Vocal. The eventual suite may contain several module-state payloads, but each payload remains independently versioned and validated.

State loading is transactional: parse and validate into a candidate first, then commit it. Malformed, truncated, non-finite or unsupported state must leave the last valid live state intact.

## Development workflow

For every new module:

`contract -> processor -> deterministic tests -> standalone wrapper -> artifact-local host probe -> REAPER engineering proof -> human usability/listening proof -> suite integration`

Do not build a feature only inside the suite and promise to extract it later.

## Current repo note

The existing `monkeys_ear` VST3 currently exposes shared Instrument/FX identities from one large wrapper. Treat that as legacy integration while modules migrate toward this standard. Do not duplicate DSP implementations to achieve separation; extract ownership around the existing DSP core.
