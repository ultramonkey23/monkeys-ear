# V7 — Subtle-by-default automatic EQ dynamics

Status: engineering guardrail research. Not perceptual validation and not production DSP.

## Product rule
The EQ should remain easy to use. Automatic dynamics are a quiet assistant, not a second instrument panel.

Default automatic behavior must be:
- subtle;
- bounded;
- transient-aware;
- low-chatter;
- reversible;
- easy to bypass;
- invisible unless the user wants to inspect it.

Stronger or unusual behavior belongs behind an explicit user action. The advanced system may expose thresholds, timing, source priority, maximum gain change, harmonic-family behavior, or state controls, but none is required for normal EQ use.

## V7 controller tested
The synthetic control-rate prototype used:
- automatic maximum gain reduction: 1.5 dB;
- persistence smoothing: 120 ms;
- nominal attack: 90 ms;
- nominal release: 260 ms;
- 30 ms transient-protection hold;
- automatic slew bound: 8 dB/s;
- nonlinear severity curve so weak evidence produces disproportionately small action.

These are experiment values, not approved production constants.

## Stress results
Compared with a deliberately naive 4 dB detector-following controller:

| scenario | auto peak GR | naive peak GR | auto transient damage | auto chatter | naive chatter |
|---|---:|---:|---:|---:|---:|
| isolated transient | 0.079 dB | 4.0 dB | 0.008 dB | 0.040 dB/s | 2.003 dB/s |
| sustained conflict | 1.204 dB | 3.4 dB | n/a | 0.593 dB/s | 1.702 dB/s |
| repeated hits | 0.127 dB | 3.6 dB | 0.080 dB | 0.277 dB/s | 16.220 dB/s |
| level step | 1.400 dB | 3.8 dB | n/a | 0.676 dB/s | 1.902 dB/s |
| priority disabled | 1.301 dB | 3.6 dB | n/a | 0.651 dB/s | 1.802 dB/s |
| silence | 0.000 dB | 0.0 dB | n/a | 0.000 dB/s | 0.000 dB/s |

A separate 30-second randomized stress signal kept automatic gain reduction below 0.952 dB, with p95 absolute reduction 0.871 dB and p99 control step 0.0082 dB. The same evidence under an explicitly user-enabled 4 dB mode reached 2.882 dB peak reduction and 2.701 dB p95 reduction.

## What this establishes
The architecture can distinguish two UX contracts:

**Auto:** conservative, low-motion assistance with a hard correction ceiling.

**User explicit:** wider timing and gain range when the user deliberately asks for stronger dynamics.

The V7 numbers only show controller behavior under synthetic evidence. They do not prove that 1.5 dB, 90 ms, 260 ms, or any other current constant sounds best.

## Design consequence
Default dynamics should not expose threshold, ratio, attack, release, resistance, harmonic ownership, or phase controls on the normal EQ surface. A normal band remains frequency / gain / Q / shape, with at most a simple dynamic amount or Auto state. Deeper controls appear only after the user expands the band or creates an explicit relationship.

The analyzer may be sophisticated internally while the interaction stays simple.

## Promotion rules
Keep the following unless listening evidence contradicts them:
1. automatic corrective gain has a conservative global and per-band ceiling;
2. brief transients receive protection unless the user explicitly targets transients;
3. weak or short-lived evidence produces little or no correction;
4. persistent evidence earns gradually stronger correction;
5. disable/bypass returns smoothly to neutral;
6. user-explicit dynamics can exceed automatic bounds;
7. no automatic harmonic pitch movement in the default lane;
8. no automatic phase or stereo manipulation in the default lane.

## Kill / revise rules
Retire a mechanism if it adds controls without a listening win, produces audible pumping/chatter, damages transient identity, requires hidden large gain changes, or cannot explain why it acted. If an old experimental mechanism survives only as a creative effect, move it out of the default corrective path rather than keeping it as hidden complexity.

## Current-practice sanity check
Modern workhorse EQs already hide significant program-dependent behavior behind simple dynamic controls. For example, FabFilter Pro-Q 4 automatically sets threshold/attack/release in spectral dynamics and exposes manual controls only when expanded. Monkey's Ear should match that usability standard while remaining more conservative and relationship-aware by default.

Reference: https://www.fabfilter.com/help/pro-q/using/spectral-dynamics
