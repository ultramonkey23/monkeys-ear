# Reconstruction, quantization and nonlinear anti-aliasing

Status: research guidance and synthetic evidence only. No local Monkey's Ear DSP was modified or verified. This file extends RESEARCH_SPINE.md and TEST_PROTOCOL.md; it does not establish a new product architecture.

## Scope and source reconciliation

The supplied Audio DSP and VST3 Development.pdf supports TPDF quantization, oversampling/polyphase filtering and ADAA. It does not establish bicubic interpolation as a preferred audio resampler. Its numerical performance tables and universal implementation claims are not product measurements. Established DSP principles are distinct from verified Monkey's Ear behavior.

External reference: Bilbao, Esqueda, Parker and Välimäki, Antiderivative Antialiasing for Memoryless Nonlinearities, IEEE Signal Processing Letters 24(7), 2017, DOI 10.1109/LSP.2017.2675541. Related: Bilbao, Esqueda and Välimäki, Antiderivative antialiasing, Lagrange interpolation and spectral flatness, WASPAA 2017, DOI 10.1109/WASPAA.2017.8170011.

## Three separate numerical problems

Interpolation reconstructs values between stored samples or control points. Anti-aliasing prevents or suppresses spectral foldover when resampling, synthesis or nonlinear processing creates out-of-band content. Dither controls quantization error when reducing numerical precision. None substitutes for the others.

### Interpolation and resampling

Compare linear, cubic and bandlimited/windowed-sinc methods on the same source and fractional positions. Cubic is not automatically bandlimited. Bicubic is a two-dimensional interpolation family; use it only where a genuine two-dimensional surface exists, such as a morph map, and validate boundaries, overshoot and parameter meaning. Do not interpolate arbitrary spectra or filter coefficients without checking stability and perceptual behavior. Fractional delay, sample playback, wavetable lookup and sample-rate conversion have different requirements. Preserve phase, amplitude, transients and stereo/mono behavior. Measure passband error, stopband rejection, aliasing, latency and CPU separately.

### Nonlinear anti-aliasing

Compare the existing implementation against native processing, 2x/4x oversampling, ADAA-1 and ADAA-1 plus 2x where applicable. ADAA is a candidate for explicit memoryless nonlinearities; it is not a universal replacement for stateful tube, feedback, filter or physical models. Include the actual interpolation/decimation filters, delay compensation and numerical near-equal-input handling. Use a high-quality bandlimited reference, nonharmonic test frequencies, multitone/IMD, sweeps and transients. Report in-band harmonic error separately from out-of-band foldover and phase/group-delay changes. Do not infer sound quality or real-time performance from an alias score alone. Higher-order ADAA and bandlimited correction methods remain optional research candidates, not mandatory implementation.

### Dither and quantization

Use a defined quantizer and TPDF baseline at intentional precision-reduction boundaries. Do not add dither indiscriminately to ordinary floating-point processing. Compare undithered, TPDF and optional noise-shaped output at 16/24-bit targets, including silence, low-level sine, decay and repeated conversion. Measure error correlation, spectral distortion, noise floor, overload and noise-shaping stability. Preserve the unquantized reference. Dither trades deterministic distortion for noise; it does not make quantization error disappear. Noise shaping is not automatically superior for every downstream process. Creative bit reduction is a separate user-controlled effect.

## Existing research integration

Pitch: retain detector/intent/expression/resynthesis separation. Add MPM/NSDF as a named deterministic comparison against YIN; compare PSOLA-style and phase-locked spectral transformation; compare LPC and cepstral/True Envelope estimation on high-F0 material. These are hypotheses, not selected product algorithms. EQ: retain conventional quality and existing ZDF/TPT work; no filter rewrite is authorized. VST3: audit parameter queues, state synchronization, latency reporting and bounded real-time work against actual imported code. SPSC is one communication option, not a universal requirement; SIMD/fusion changes require profiling.

## Evidence and promotion

Keep synthetic results SYNTHETIC. A reference-paper result does not prove a local implementation. Preserve rejected experiments and limitations. Before product integration inspect the local source, run existing tests, compare real audio, measure CPU/latency and obtain listening evidence where sound quality is claimed. No new framework, module, or Lab authority is created by this research.
