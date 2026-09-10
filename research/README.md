# Monkey's Ear research

This repository is currently a **research shell**, not yet the complete Monkey's Ear product source. A more complete local implementation predates this GitHub repository and must be imported/audited before product-integration decisions.

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

## Integration rule

When the local Monkey's Ear source arrives:
1. inspect/import current truth;
2. map existing implementations to the research hypotheses;
3. preserve working code rather than replacing it from assumptions;
4. dogfood the shared test corpus through real builds;
5. translate verified evidence into implementation tasks through the existing Lab/Code Prime ownership path;
6. promote only changes that survive engineering and listening evidence.
