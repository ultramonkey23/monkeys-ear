# Monkey's Ear — Pitch Correction Research

This directory is the durable research area for Monkey's Ear pitch correction / pitch control.

Read:
- `ARCHITECTURE.md` — current pre-integration research direction.
- `RESEARCH_HISTORY.md` — evidence, baselines, failures, and open questions.
- `../TEST_PROTOCOL.md` — shared same-source comparison and listening protocol.
- `../RESEARCH_SPINE.md` — product-wide laws and integration constraints.

## Source-of-truth warning

The current GitHub repository is **not yet the complete Monkey's Ear product source**. A more complete local implementation existed before this GitHub repository was created, and that local code has not yet been imported and audited here. Therefore absence of code or features in GitHub must not be interpreted as proof that they do not exist in Monkey's Ear.

Until the local implementation is brought in, this folder is **pre-integration research only**. Do not design around assumptions about missing product code, interfaces, DSP paths, UI, or existing pitch facilities.

## Scope

The goal is not to clone Auto-Tune. The goal is a controllable pitch-correction engine that can correct intonation while preserving useful expression, timbre, consonants/noise, and musical transitions, and whose output can become a standardized source for testing other Monkey's Ear processors.

## Workflow

1. inspect the imported/local product truth before integration or implementation decisions;
2. compare against strong external baselines on the exact same dry recorded source;
3. separate detection, musical target, expressive shape, and resynthesis in tests;
4. run synthetic and recorded-audio experiments before promoting mechanisms;
5. preserve executable/data evidence but avoid versioned design-doc accumulation;
6. keep one active architecture document once the local implementation has been audited;
7. preserve concise research history, including failed and demoted mechanisms.

## Current state

Research initialized and reconciled with the Lab's existing audio-process knowledge. GitHub product-code coverage is still incomplete and must not be treated as authoritative. Local implementation audit/import, executable pitch simulations, same-track external-tool comparison, product integration, and listening validation are pending.
