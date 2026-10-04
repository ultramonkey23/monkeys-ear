# Monkey's Ear research

**Research subsystem guide, not a claim that the repository contains research only.** As inspected on `master` (2026-10-04), this same repository already contains a buildable-source *structure* for the native C++ audio engine (`src/`, `include/`), VST3 entry wrappers (`vst3/`), host/deterministic test sources (`test/`), CMake build definitions, presets, and vocal packaging. Their presence establishes source implementation, **not** a newly executed host build or evidence that every feature from Cody's earlier local implementation has been imported. That historical-source completeness remains UNKNOWN pending direct comparison.

## Read first

- `RESEARCH_SPINE.md` — product-wide laws, source-truth ladder, evidence/learning gate, subsystem workflow and real-time constraints.
- `TEST_PROTOCOL.md` — shared source corpus, external-tool comparisons, listening and measurement protocol.
- `STATUS.json` — machine-readable current research/subsystem status.
- `EVIDENCE_SCHEMA.json` — machine-readable research evidence/outcome contract.

## Active subsystem research

- `onion-skin-eq/` — EQ / spectral dynamics / relational evidence.
- `pitch-correction/` — pitch correction / pitch control.

Each active subsystem should converge toward:
- a small `README.md` for navigation/scope;
- one `ARCHITECTURE.md` for current design truth;
- one `RESEARCH_HISTORY.md` for evidence, failures and demotions;
- executable scripts/data for reproducibility.

Do not accumulate `V10_DESIGN.md`, `V11_DESIGN.md`, and similar permanent guidance files. Experiments may be versioned in code/data, but active design must stay consolidated.

## Source truth

Use this order when sources disagree:

1. Cody's newest explicit direction.
2. Local/imported Monkey's Ear implementation and live runtime.
3. Build/runtime/audio/profiler evidence.
4. Verified reproducible experiments.
5. GitHub research docs.
6. Chat/agent recollection.

Weaker proof never outranks stronger proof. GitHub absence is not proof that the pre-existing local implementation lacks a feature.

## Lab relationship

Ultramonkeydog Lab already contains reusable audio-process knowledge: deterministic evidence/provenance, research packets and source scoring, verified learning gates/outcome history, prior Living Audio Forge work, REAPER as a human workbench, framework-independent native cores, and Code Prime as the intentional code-mutation boundary.

Monkey's Ear should reuse those capabilities and concepts rather than create a parallel audio authority. `EVIDENCE_SCHEMA.json` is deliberately small so future Lab tooling can ingest/translate research outcomes without Monkey's Ear cloning OutcomeJournal or Learning Gate.

## Research-to-product integration rule

1. Start from **current native product source in this repository** plus the exact canonical branch/HEAD and real evidence, rather than treating this checkout as an empty research shell.
2. Compare a proposed research mechanism against the existing processor, parameter host surface, preset compatibility, VST3 wrapper, tests, and any relevant older local implementation **when that source is available**. Do not assume the older local body has been completely imported.
3. Preserve functioning processing and host automation/parameter IDs; do not silently fork the product DSP to accommodate a research experiment.
4. Evaluate hypotheses with reproducible measurement and meaningful listening in REAPER or the relevant DAW, with clear evidence/unknown labels. Test presence is not sound-quality acceptance.
5. Route implementation through the existing Lab/Code Prime ownership when developing under the Lab, but keep native product code and canon owned by Monkey's Ear; research candidates do not auto-promote.
6. Keep `research/STATUS.json` and research architecture pages aligned with *source-observable research status* and separate legacy-import completeness from implementation presence.

**Branch truth at inspection:** `master` is the declared production branch in root `AGENTS.md`. At the sampled time GitHub's default `main` branch trailed `master` by two commits (master `8d58504d0b1f...`, main `d85357388c25...`). Before treating a GitHub default-branch page as fresh product evidence, resolve branch ancestry and actual current refs; do not invent a different branch policy or assume the two are synchronized.
