# Monkey's Ear research

This repository is currently a **research shell**, not yet the complete Monkey's Ear product source. A more complete local implementation predates this GitHub repository and must be imported/audited before product-integration decisions.

## Read first

- `RESEARCH_SPINE.md` — product-wide laws, subsystem workflow, real-time constraints and durable concepts.
- `TEST_PROTOCOL.md` — shared source corpus, external-tool comparisons, listening and measurement protocol.

## Active subsystem research

- `onion-skin-eq/` — EQ / spectral dynamics / relational evidence.
- `pitch-correction/` — pitch correction / pitch control.

Each active subsystem should converge toward:
- a small `README.md` for navigation/scope;
- one `ARCHITECTURE.md` for current design truth;
- one `RESEARCH_HISTORY.md` for evidence, failures and demotions;
- executable scripts/data for reproducibility.

Do not accumulate `V10_DESIGN.md`, `V11_DESIGN.md`, and similar permanent guidance files. Experiments may be versioned in code/data, but active design must stay consolidated.

## Lab relationship

Ultramonkeydog Lab already contains reusable audio-process knowledge: deterministic evidence/provenance, prior Living Audio Forge work, REAPER as a human workbench, framework-independent native cores, and Code Prime as the intentional code-mutation boundary. Monkey's Ear research should reuse those capabilities and rules rather than create a parallel audio authority.

## Integration rule

When the local Monkey's Ear source arrives:
1. inspect/import current truth;
2. map existing implementations to the research hypotheses;
3. preserve working code rather than replacing it from assumptions;
4. dogfood the shared test corpus through real builds;
5. promote only changes that survive engineering and listening evidence.
