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
