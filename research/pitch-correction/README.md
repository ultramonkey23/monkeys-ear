# Monkey's Ear — Pitch Correction Research

This directory is the durable research area for Monkey's Ear pitch correction / pitch control.

Start with `ARCHITECTURE.md` for current direction and `RESEARCH_HISTORY.md` for evidence, baselines, failures, and decisions.

## Scope

The goal is not to clone Auto-Tune. The goal is a controllable pitch-correction engine that can correct intonation while preserving useful expression, timbre, consonants/noise, and musical transitions, and whose output can become a standardized source for testing other Monkey's Ear processors.

## Workflow

1. inspect current repo truth before adding code;
2. compare against strong external baselines on the same dry recorded source;
3. separate detection, musical target, expressive shape, and resynthesis in tests;
4. run synthetic and recorded-audio experiments before promoting mechanisms;
5. preserve executable/data evidence but avoid versioned design-doc accumulation;
6. update `ARCHITECTURE.md` only when active design changes;
7. update `RESEARCH_HISTORY.md` with concise evidence and demotions.

## Current state

Research initialized. No existing pitch-correction implementation was found in the current repository at the start of this pass. Product integration and listening validation are pending.
