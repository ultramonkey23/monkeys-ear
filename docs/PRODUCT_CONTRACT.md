# PRODUCT_CONTRACT.md — Monkey's Ear Product Laws & Intent

## 1. Product Thesis
Monkey's Ear is an intentional, production-focused live digital instrument and sound-design workbench combining synthesis, sampling, live audio/vocal processing, physical/nonlinear modeling, modulation, amplification, delay, reverb, and semantic control.

## 2. Product Anti-Tunnel-Vision Law
Monkey's Ear is NOT:
- a latency research project;
- an ML project;
- a chronofrequency project;
- a rollback-netcode experiment;
- a physical-modeling project;
- a semantic-AI project;
- a modular synth research toy.

The product is:
> A production-grade live instrument and sound environment that is fast enough to perform through, deep enough to produce with, controllable enough for deliberate music making, weird enough to discover genuinely new sound, and structurally strong enough to keep growing.

## 3. The 10 Musical Axes
At every planning decision, evaluate:
1. Production usefulness
2. Live playability
3. Sound quality
4. Deliberate musician control
5. Deterministic recall
6. Workflow
7. Originality
8. Extensibility
9. CPU / Latency
10. Actual human musical usefulness

## 4. Operating Tiers
- **LIVE**: Causal, strictly bounded, lowest-latency algorithms. 0 samples reported latency.
- **STUDIO**: Higher fidelity, modest explicitly declared latency/oversampling tradeoffs.
- **RENDER**: Expensive reference physical models, large windows, offline non-causal processing.

Controls retain identical musical meaning across tiers even if internal execution changes.

## 5. Machine Learning Law
Any ML in Monkey's Ear must be small, streamlined, powerful, task-specific, with explicit inputs/outputs, fixed resource budget, and deterministic DSP fallback. The engine must continue producing valid audio if every learned model is disabled.

## 6. Standalone Android / Mobile DAW Product Evolution (Cody direction, 2026-11-08)

Monkey's Ear's product family will expand beyond the REAPER-hosted VST3 instruments/effects toward a **standalone Android app, eventually a full mobile DAW**. This is a future product requirement, not a current Android implementation or release claim.

- The interface is **touch-first**, not a miniaturized desktop DAW. Piano roll, step sequencing, instrument interaction, loop and arrangement editing must be designed around precise touch gestures, with reliable selection/editing, zoom, visible feedback and accessible fine controls.
- Support **external MIDI instruments/controllers and MIDI recording/playback**, **touch performance**, and **ordinary physical/computer keyboard performance input** for musicians without a MIDI controller. Touch percussion pads do not replace a proper keyboard/controller for rhythmically precise playing. Musical input paths must converge on consistent editable note/timing semantics.
- Support **pattern-based, loop-based, sequenced and live workflows**, suitable for composing, performing, recording, overdubbing and developing arrangements. Offer strong melodic/piano-roll and drum/step sequencer options, rather than reducing the DAW to one pattern grid.
- **Deep quantization** is a musical instrument, not just a hard snap switch: live/after-recording choices, editable timing and strength, musical divisions and groove/swing, preservation of expressive microtiming, and visibility/undo of timing changes. Specific algorithm/UI designs must be judged by real play evidence.
- **Share sounds and compatible presets** between mobile and the REAPER ecosystem; **synchronized audio stem export/import bridges continued audio editing** in REAPER. Do not silently substitute full project parity for the stated stem bridge or promise preset bit-identity before validating the participating engines.
- Mobile **must create and edit music without AI**. Any AI assist is optional enhancement, with no necessary model, network or account dependency for core production.
- Preserve the distinct connected REAPER ecosystem and its live-audio contracts. Determine code sharing, engine reuse, file schemas and actual device performance from the current source and evidence before choosing a mobile implementation strategy.

Expanded musician-owned product canon and proposed verification questions: [`MOBILE_DAW_PRODUCT_VISION.md`](MOBILE_DAW_PRODUCT_VISION.md).

## 7. Production-Inclusive Experimentation & Canonical Branch

**Prototyping and experimentation are part of the full Monkey's Ear production process.** Do not invent a separate non-production product, speculative prototype branch, disposable implementation or forced handoff that makes one creator track multiple competing systems. Experimental features can have explicit proof/release-readiness states **within the canonical production body**; no premature shipping claim follows from their presence.

The **sole canonical development/integration branch is `master`** under the Lab's established ownership and mutation rules. Do not create feature/transport branches, PR fallbacks or a separate `main` development lane. Inspect live source and concurrent work before mutations; commit and normally push accepted work on `master` and verify remote readback. GitHub's configured default branch may still be `main` until deliberately corrected; never interpret that UI default as overriding canonical `master`. Any residual `main` compatibility alias must not diverge, and repository administration/default-branch correction requires the authorized mechanism.
