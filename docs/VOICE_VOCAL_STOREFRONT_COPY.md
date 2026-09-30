# Monkey's Ear Voice/Vocal — storefront copy

> **STATUS: PRE-PUBLISH DRAFT.** This file is the customer-facing copy owner for
> the first standalone Monkey's Ear release candidate. It must stay narrower
> than the unfinished suite and must not be treated as proof that the remaining
> release gates have passed.

## Itch title

**Monkey's Ear Voice/Vocal — Windows VST3 Preview**

## Short description

A low-latency Windows VST3 vocal-expression processor for deliberate pitch
correction, drift/vibrato shaping, transitions, envelope repair, spectral
character, and dry/wet control.

## Store page copy

**Voice/Vocal is the first standalone release candidate from Monkey's Ear.**

Monkey's Ear is being built as a larger ecosystem of instruments and effects.
That full suite — including the eventual integrated Monkey's Ear instrument —
is still in development. This download is **Voice/Vocal only**.

Voice/Vocal is a stereo Windows x64 VST3 effect built around causal vocal
analysis and expression shaping. The current preview exposes 11 automatable
controls:

- Enable / bypass
- Correction Strength
- Drift Retention
- Vibrato Retention
- Transition
- Envelope Repair
- Spectral Residual
- Character
- Mix
- Soft Sequential Staging
- Aperiodic Protection

The goal is not to flatten every voice into one robotic result. The controls
separate correction strength from retained movement and texture so the user can
choose how much pitch drift, vibrato, transitions, envelope shape, residual
spectrum, and aperiodic material survive the process.

The current LIVE path reports **0 samples of plug-in latency** through the VST3
processor. The plug-in also stores its 11-parameter state through a versioned
component/controller state contract, and the automated validation path rejects
truncated or non-finite state rather than partially applying corrupt recall
data.

Those are engineering facts from the current validation path. They are **not**
a claim that every DAW, machine, voice, or production session has already been
proven.

### What is included

The Windows validation/release package is intentionally small:

- `monkeys_ear_vocal.vst3`
- exact build/commit identity
- Voice/Vocal version
- changelog
- install/remove instructions
- known-limit and failure-report notes
- SHA-256 checksum information

You are not required to install the unfinished Monkey's Ear suite.

### Current interface

This preview uses the host's standard VST3 parameter interface. A dedicated
Monkey's Ear visual skin is not part of this preview yet.

### Platform

- Windows x64
- VST3 effect
- A VST3-compatible audio host is required

Host compatibility must be claimed from real host testing, not from the VST3
format name alone.

### Install

1. Close your audio host.
2. Copy `monkeys_ear_vocal.vst3` to:
   `C:\Program Files\Common Files\VST3\`
3. Re-open your host and re-scan VST3 plug-ins if necessary.

Windows may require administrator permission to copy or remove files from the
system VST3 folder.

### Preview / early-access boundary

This is an early Voice/Vocal product boundary, not the finished Monkey's Ear
ecosystem.

Before broad compatibility or production-readiness claims are made, the exact
downloadable build still needs real clean-user host proof, project
save/reopen/render proof, session CPU observation, before/after listening
examples, and musician usability feedback.

If the build behaves unexpectedly, bypass it and reduce monitoring level.
Signal protection inside software is not a hearing-safety guarantee.

### Reporting a problem

When the public Itch page is enabled for support/feedback, include:

- Voice/Vocal version
- build/commit identity from the package
- Windows version
- host name and version
- sample rate and buffer size
- exact reproduction steps
- expected result
- actual result
- whether audio became silent, distorted, unstable, or unexpectedly loud

Checksum information from the package should be kept with any report so the
exact build can be identified.

## Product-family boundary

Use this hierarchy consistently in screenshots, posts, release notes, and
store text:

1. **Monkey's Ear** — the evolving product family/ecosystem.
2. **Voice/Vocal** — this standalone vocal-processing preview/release candidate.
3. **Integrated Monkey's Ear instrument** — future-facing; not included here.

Do not imply that buying/downloading Voice/Vocal includes or proves the full
suite.

---

## Internal publish gate — do not paste into the storefront

This copy can be prepared before publication, but a paid/public release must
not be represented as cleared until the remaining issue #1 gates are satisfied.

Still required before accepting money or broadening claims:

- choose explicit end-user distribution/license terms;
- place the exact package at a stable non-expiring download location;
- clean-user REAPER scan/load/save/reopen/render proof on the exact package;
- one additional real host smoke test when available, or an explicit
  REAPER-only compatibility boundary;
- exact-package before/after audio;
- exact-package screenshot or short screen recording;
- one concrete public support/contact route on the live listing;
- first real-user install/listening/usability evidence.

Pricing is deliberately not specified in this document. Price and commercial
terms are a product decision, not something source validation can invent.
