# Monkey's Ear — EQ research

This folder contains research for the **EQ / spectral-dynamics module only**. It does not define the full Monkey's Ear system.

## Read order

1. `ARCHITECTURE.md` — current design source of truth.
2. `RESEARCH_HISTORY.md` — concise evidence/provenance ledger from V1–V9.
3. Reproducible `.py` / `.csv` artifacts — experiments and measurements, not design authority.

Do **not** treat old experiment names, synthetic thresholds, collision scores or ERB-overlap scores as product requirements or perceptual truth.

## Current direction

The EQ remains a high-quality conventional EQ first. Deeper research focuses on dynamic EQ/compression math, transient/body evidence, harmonic-family analysis, directional Preserve / Yield / Equal relationships, onion-skin history and **projected bounded action**: propose a small correction, evaluate it, and reject it if it does not help or violates protected structure.

Automatic behavior is secondary. Advanced mechanisms must remain user-controllable, inspectable, bypassable, automatable, reversible and recallable.

## Documentation rule

Do not create a new permanent design document for every experiment generation. Update `ARCHITECTURE.md` when active design changes and append the durable result to `RESEARCH_HISTORY.md`. Keep executable evidence when useful for reproduction. Delete duplicated narrative once its surviving conclusions are consolidated.

Complexity is not progress. New mechanisms must beat a simpler baseline for a defined task, win a controlled listening test, or provide a genuinely distinct creative capability.