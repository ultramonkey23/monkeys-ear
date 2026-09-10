# Vocal Refinement Research — 2026-09-10

Status: research guidance, not listening proof or release validation.

## Scope lock

The vocal module is feature-complete in *families*. Do not add a conventional channel strip. Refine and integrate the established systems: correction, independent pitch shift, custom/microtonal tuning, articulation/phrase behavior, vibrato/drift/transition control, source-aware resynthesis, formant/identity transformation, performance-aware compression, and component-aware saturation. After convergence and perceptual validation, return priority to the synth.

## Highest-value architectural finding: shared vocal state

The existing processors should not grow independent detectors. Introduce a bounded `VocalAnalysisFrame` (or equivalent owner) that describes the current performance once and is consumed by targeting, resynthesis, dynamics, and character stages.

Recommended evidence groups:

- pitch: observed F0, confidence, center estimate, motion;
- expression: slow drift, vibrato depth/rate/phase confidence, transition energy;
- source: periodic/mixed/aperiodic weights rather than only hard labels;
- phrase: onset, release, stability, connection, target age;
- dynamics: peak/RMS or similarly cheap envelope evidence;
- spectral identity: a bounded spectral-envelope/formant representation when available.

This should remain fixed-memory, causal in LIVE mode, and cheap enough to share rather than recompute.

## 1. Decompose pitch behavior before transforming it

Treat observed pitch in log-frequency/cents space as approximately:

`P(t) = C(t) + D(t) + V(t) + T(t) + R(t)`

where `C` is musical center, `D` slow drift, `V` quasi-periodic vibrato, `T` transition/portamento/ornament motion, and `R` residual instability/uncertainty.

The decomposition need not be perfect transcription. Its purpose is control separation. Correction should primarily modify `C`; drift retention scales `D`; vibrato control scales/reshapes `V`; transition preservation acts on `T`; character may retain or exaggerate bounded `R`.

A practical causal first generation can use multiple time scales plus confidence gating rather than ML. Do not classify low-confidence F0 noise as vibrato.

## 2. Correction and transposition are different intents

Keep target correction and intentional pitch shifting independent in cents/log-frequency space:

`P_out(t) = P_source(t) + C_correction(t) + C_transpose(t) + C_character(t)`

The musical target engine should choose the corrected trajectory before intentional transposition is applied, so a -700 cent transpose does not cause the target selector to reinterpret the singer's note identity.

This also permits MIDI/manual/automation transpose without corrupting scale logic.

## 3. Target gravity should split into musical functions

The current scalar gravity is useful but risks doing several jobs simultaneously. Evolve only if experiments show the limitation. Candidate internal dimensions:

- destination attraction: likelihood of becoming a stable target;
- capture radius: how far the degree may attract a candidate;
- passing affinity: how willing a trajectory is to travel through the degree without settling;
- ascending/descending affinity: directional behavior;
- release resistance: how strongly an established degree resists switching.

This permits a color/passing degree to be musically important without becoming a sustained-note magnet. Roles such as ANCHOR/STABLE/COLOR/PASSING/APPROACH can be macros over these continuous values rather than hard-coded theory.

## 4. Phrase state should be continuous evidence first

Avoid a brittle giant note-state enum. Maintain bounded evidence such as onset probability, release probability, stability, connection, motion intent, periodicity and target age. Higher-level ATTACK/STABLE/TRANSITION/ORNAMENT/RELEASE/UNCERTAIN behavior can be derived from these signals.

Examples:

- high onset + low connection -> release target history, protect transient, fast reacquisition;
- low onset + high connection + high motion -> preserve portamento and resist intermediate target capture;
- stable center + periodic modulation -> preserve/reshape vibrato without target switching.

## 5. Formant/identity architecture: source, filter, residual

Do not make formant control merely an ON/OFF correction flag. Separate conceptual components:

1. periodic excitation/source;
2. spectral envelope / vocal-tract identity;
3. aperiodic/transient residual.

PSOLA remains well suited to low-latency periodic pitch transformation. Spectral-envelope estimation should own identity preservation/transformation. Aperiodic material should normally avoid harmonic repitching.

Useful internal controls:

- formant shift;
- formant follow (how much envelope follows pitch transpose);
- formant stretch/warp;
- identity restoration strength;
- creative envelope character.

Cepstral low-pass liftering is a strong bounded candidate for envelope extraction; LPC/source-filter is another candidate worth comparative testing. Do not replace the current path until a controlled experiment shows an advantage.

## 6. Adaptive transform staging

Do not hard-code 'two PSOLA stages is better'. Let transform difficulty decide the path. A small correction should take the cheapest clean route. Larger shifts may distribute correction across softer stages and optionally invoke envelope/residual repair.

Candidate difficulty inputs:

- absolute cents shift;
- F0 confidence;
- periodicity;
- transient proximity;
- requested formant preservation;
- articulation/transition state.

The planner should remain bounded and deterministic in LIVE mode.

## 7. Performance-aware compression

Keep a conventional, trustworthy feed-forward gain computer, but let shared vocal evidence steer its ballistics and weighting. Classical compressor research supports independent detector and gain-smoothing attack/release; perceptual research indicates attack timing is especially influential.

Recommended architecture: two cooperating envelopes rather than one magical detector.

- FAST SAFETY/PRESENCE path: catches excursions and protects peaks.
- SLOW BODY/DENSITY path: stabilizes sustained vocal body.

Phrase/source evidence can then modify behavior:

- consonant/transient: reduce slow-body influence and preserve attack;
- stable vowel: increase density control;
- vibrato: avoid amplitude-modulation pumping;
- connected transition: maintain gain continuity;
- breath/aperiodic material: reduce detector weighting when appropriate.

Blend requested gain reduction in dB with bounded rules; do not simply sum two aggressive compressors.

## 8. Component-aware saturation

Nonlinear waveshaping aliases when generated harmonics exceed Nyquist. ADAA is a serious candidate for low-latency saturation, and recent DAFx work demonstrates numerical antiderivatives for LUT-defined nonlinearities, allowing arbitrary measured/designed curves without requiring a closed-form antiderivative.

Recommended progression:

1. begin with a well-behaved analytic curve and first-order ADAA;
2. compare alias rejection and CPU against light oversampling;
3. consider ADAA + light oversampling for higher quality modes;
4. only then investigate LUT-defined asymmetric/custom curves.

Make drive component-aware:

- periodic body: full harmonic enrichment;
- mixed source: reduced drive;
- aperiodic/sibilant/transient material: protected or lightly driven by default.

Creative modes may deliberately violate this, but the violation must be bounded and recallable.

## 9. Dynamics + saturation can form a shared DENSITY system

Do not force the musician to understand every internal stage. Clean compression and nonlinear saturation both alter perceived density. A high-level DENSITY macro can coordinate them while advanced controls remain available.

As nonlinear drive contributes more peak control/body, clean compressor behavior can back off. Keep final safety limiting independent.

This should be evaluated perceptually; it is a design hypothesis, not an established winner.

## 10. Validation experiments worth doing after architecture lands

Do not run a huge proof campaign now. When the relevant mechanisms exist, use small discriminating experiments:

- pitch decomposition: synthetic contours containing independent drift + vibrato + slides; measure leakage between recovered components;
- target field: adversarial vibrato boundary, passing tone, slow slide, repeated note, asymmetric scale and non-octave trajectories;
- formant: compare cepstral envelope vs LPC preservation at small/medium/extreme shifts;
- staging: one-stage vs adaptive multi-stage at matched total cents shift;
- compression: transient preservation, stable-body variance, modulation pumping;
- saturation: swept sine alias energy and CPU for naive, ADAA, oversampled, ADAA+light-OS paths.

These are development discriminators. None substitutes for listening.

## Research basis / external directions

Useful references researched for this wave:

- Bilbao, Esqueda, Parker, Valimaki — Antiderivative Antialiasing for Memoryless Nonlinearities (IEEE SPL, 2017).
- Parker, Zavalishin, Le Bivic — reducing aliasing of nonlinear waveshaping using continuous-time convolution (DAFx-2016).
- Gabrielli & Squartini — Simplifying Antiderivative Antialiasing with Lookup Table Integration (DAFx-2025): practical numerical integration of LUT nonlinearities.
- Bitzer, Schmidt & Simmer — Parameter Estimation of Dynamic Range Compressors (AES 120): feed-forward model with independent RMS detection and gain-smoothing attack/release.
- AES work on compressor ballistics: attack setting has strong perceptual influence on perceived style.
- Real-time/open DSP implementations of STFT pitch/timbre shifting demonstrate cepstral spectral-envelope preservation as a practical comparison path.

## Next implementation order

1. Shared vocal-analysis state / ownership.
2. Causal center-drift-vibrato-transition decomposition.
3. Phrase/articulation evidence integrated into target selection.
4. Refined target-field semantics only where current gravity demonstrably fails.
5. Independent intentional transpose trajectory.
6. Formant/source-filter/residual comparison and integration.
7. Adaptive transform staging.
8. Performance-aware dual-path dynamics.
9. ADAA/component-aware saturation.
10. Integration tuning, listening, DAW comparison, then freeze vocal feature expansion and return to synth work.
