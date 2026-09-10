# Synthetic quality experiments

Run `python run.py` with NumPy, SciPy and pandas. The script writes interpolation.csv, dither.csv and nonlinear.csv. These are independent research prototypes, not Monkey's Ear DSP. No listening, native build, CPU benchmark or local integration has been performed.

## Interpolation

48 kHz, one-second sine, fractional offset 0.37 samples. Linear, Catmull-Rom cubic and normalized 33-tap Lanczos-windowed sinc are compared against the exact sine. RMSE:

| Hz | Linear | Cubic | Sinc33 |
|---:|---:|---:|---:|
|1000|0.001412|0.000017|0.000539|
|6000|0.050172|0.006343|0.000230|
|12000|0.192881|0.077727|0.000559|
|18000|0.405919|0.296907|0.000984|

Cubic is a useful inexpensive candidate, not a bandlimited replacement. Sinc33 is not universally best; its finite window and normalization affect low-frequency accuracy. Test actual resampling ratios, phase, transients and CPU before selection.

## Quantization

8-bit signed-equivalent step 1/128, 0.2-amplitude 997 Hz sine, 48 kHz, one second, independent TPDF random draws, seed 20260907. Undithered error RMS 0.002352 and input/error correlation 0.026791; TPDF error RMS 0.003902 and correlation -0.000189. Dither increased noise while reducing this measured correlation. One signal is insufficient to establish universal decorrelation or perceptual superiority. Test low-level tones, silence, decays, 16/24-bit targets and noise shaping separately.

## Nonlinear reference comparison

48 kHz, one-second periodic sine, drive 3 into tanh, frequencies 997/5999/11999 Hz. Native, ADAA-1, 2x, 4x and ADAA-1+2x use SciPy polyphase resampling with periodic extension. Reference is a 65536-point Fourier analysis of tanh(3 sin phase), reconstructed from harmonics strictly below Nyquist. ADAA's half-sample delay is compensated in the frequency domain; no gain fitting. RMSE:

| Hz | Native | ADAA1 | 2x | 4x | ADAA1+2x |
|---:|---:|---:|---:|---:|---:|
|997|0.00000|0.00245|0.00046|0.00056|0.00046|
|5999|0.07397|0.06525|0.00147|0.00077|0.01482|
|11999|0.21402|0.13180|0.02666|0.00135|0.02737|

These are synthetic reference errors, not isolated alias-energy measurements. The near-zero native result at 997 Hz reflects harmonic folding collisions and is not evidence of zero aliasing. The 2x/4x comparison uses the same default resampling filter family, not optimized production filters. No universal ADAA superiority is established.

## Rejected exploratory measurements

An initial 16x-render reference and gain-fitted RMSE comparison was rejected because it confounded reference quality, phase and amplitude. A subsequent alias-bin mask was rejected because integer-harmonic test frequencies allow aliases to collide with legitimate harmonic bins, and resampling edges contaminate the spectrum. Neither rejected result should be used to select an algorithm. The corrected experiment above uses nonharmonic frequencies and an analytic Fourier reference, but still requires a separate alias/phase/CPU evaluation.

## Next research gate

Use actual imported DSP and identical source material. Compare interpolation, nonlinear anti-aliasing and quantization independently. Preserve reference renders and filter settings; measure aliasing, passband error, phase/group delay, latency, CPU and real audio listening separately. Do not promote these synthetic results to product verification.
