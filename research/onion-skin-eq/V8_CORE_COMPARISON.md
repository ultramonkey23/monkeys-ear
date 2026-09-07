# V8 — Core EQ strategy comparison

Status: synthetic engineering research for the **Monkey's Ear EQ module only**. This is not product DSP, perceptual masking validation, or listening proof.

## Question

Which core intervention best improves separation between overlapping textures while preserving source identity, energy, and stable behavior?

The four strategies compared are:

1. broad static EQ carving;
2. conventional dynamic EQ/compression-style band gain;
3. harmonic-family gain control;
4. relational state dynamics using conflict, persistence, ownership, user priority, transient protection, and inertia.

## Main result

In the deterministic matched case, the untouched collision proxy was 0.3292.

| Strategy | Collision improvement | Identity floor | Mean energy delta | Control-motion index |
|---|---:|---:|---:|---:|
| Broad static carve | 4.58% | 0.9970 | -0.160 dB | 0.750 |
| Conventional dynamic EQ | 2.72% | 0.9985 | -0.112 dB | 0.649 |
| Harmonic-family control | 1.14% | 0.9998 | -0.060 dB | 0.154 |
| Relational state dynamics | 0.90% | 0.9998 | -0.061 dB | 0.167 |

A 2,000-state randomized sweep produced the same broad tradeoff: broad carving achieved the most raw reduction in the collision proxy, while harmonic-family control preserved identity much better with substantially less motion. The current relational-state prototype was more conservative still, but did not yet earn its extra complexity.

These metrics are not perceptual scores. In particular, the collision proxy rewards carving and therefore cannot be the sole optimization target.

## Decisions

**PROMOTE — harmonic-family gain negotiation.** It currently offers the strongest evidence for a distinctive EQ mechanism: low collateral identity change and low control motion. It should advance to real-audio and listening tests.

**KEEP AS BASELINE — conventional dynamic EQ.** It is mature, understandable, and a required comparison target. New mechanisms must beat it for a defined musical purpose, not merely look more sophisticated.

**KEEP AS EXPLICIT TOOL — broad carving.** It is highly effective when the user actually wants separation. Do not classify its larger identity/energy change as automatically bad; it is the correct tool in many mixes. It remains a baseline, not the desired automatic optimizer.

**REVISE / EXPERIMENTAL — relational state dynamics.** The idea remains promising for transient protection, persistence, user priority, resistance, and return, but V8 does not show enough additional benefit to justify making the whole state system the core. Split useful pieces out and retest them independently.

**RETIRED AS AN OBJECTIVE — maximize spectral separation.** Separation alone over-rewards destructive carving. The EQ must optimize a multi-objective tradeoff: intended separation, source identity, energy/timbre preservation, transient preservation, stability, and user intent.

## Core design consequence

The EQ should not have one giant "smart" control law. Use a layered architecture:

- conventional filter engine;
- analysis/evidence;
- dynamic gain engine;
- harmonic-family grouping;
- optional relationship controls;
- optional state smoothing/persistence;
- advanced experimental phase/spatial/harmonic-motion actions.

Each layer must be individually bypassable, measurable, and removable.

## Next gate

Before promotion into product DSP:

1. replace the collision proxy with a documented auditory-filter/excitation baseline;
2. run the same four-way comparison on real stems;
3. level-match outputs;
4. blind-listen for clarity, depth, punch, timbral damage, and preference;
5. record CPU, latency, worst-case gain motion, zippering, mono behavior, and phase effects;
6. remove or demote any mechanism that fails to beat a simpler baseline for a defined task.

## Durability rule

Complexity is not progress. A mechanism remains active only if it either:
- wins a measured engineering tradeoff,
- wins a controlled listening task,
- or provides a clearly different user-controlled creative capability.

Otherwise simplify, revise, archive, or remove it.
