"""Monkey's Ear hybrid vocal transform research v1.

Evidence state: SYNTHETIC.
Compares:
- expanded multi-cycle PSOLA-style baseline,
- identity-phase-locking-style spectral baseline,
- adaptive hybrid routing between the two.

The corpus uses deterministic synthetic source/filter vowels with controlled
periodicity, breath/noise, transients and pitch-shift magnitudes. Metrics are
against an oracle synthetic target with unchanged formant geometry.

This is not production DSP or listening proof.
"""
import numpy as np
import pandas as pd
from pathlib import Path
from scipy.signal import resample
from scipy.io import wavfile
import librosa

FS = 48000
OUT = Path(__file__).resolve().parent
RNG = np.random.default_rng(20260907)

VOWELS = {
    "a": [(730, 90, 1.0), (1090, 110, .72), (2440, 180, .45)],
    "i": [(270, 70, 1.0), (2290, 140, .65), (3010, 180, .42)],
    "u": [(300, 80, 1.0), (870, 100, .72), (2240, 170, .40)],
}

def env_curve(f, formants):
    f = np.asarray(f, float)
    y = np.full_like(f, .015)
    for fc, bw, gain in formants:
        y += gain / (1 + ((f - fc) / (bw / 2)) ** 2)
    y *= 1 / np.sqrt(np.maximum(f, 80) / 80)
    return y

def synth_segment(f0, formants, dur=.5, noise=.0, transient=False):
    t = np.arange(int(FS * dur)) / FS
    h = np.arange(1, int((FS / 2 - 500) // f0) + 1)
    freqs = h * f0
    amps = env_curve(freqs, formants)
    phases = (h * .37) % (2 * np.pi)
    y = np.sum(
        amps[:, None] * np.sin(2 * np.pi * freqs[:, None] * t + phases[:, None]),
        axis=0,
    )
    if noise:
        n = RNG.standard_normal(len(t))
        n = np.r_[0, np.diff(n)]
        n /= max(np.std(n), 1e-12)
        y += noise * n
    if transient:
        n = int(.012 * FS)
        y[:n] += np.hanning(2 * n)[:n] * RNG.standard_normal(n) * 2.5
    edge = int(.02 * FS)
    ramp = np.linspace(0, 1, edge)
    y[:edge] *= ramp
    y[-edge:] *= ramp[::-1]
    y /= max(np.max(np.abs(y)), 1e-12) * 1.02
    return y

def expanded_psola(x, f0, ratio, cycles=2.0):
    period = FS / f0
    expected = np.arange(int(2 * period), len(x) - int(2 * period), period)
    radius = max(2, int(.18 * period))
    marks = []
    for expected_mark in expected:
        i = int(round(expected_mark))
        lo, hi = max(0, i - radius), min(len(x), i + radius + 1)
        marks.append(lo + int(np.argmax(np.abs(x[lo:hi]))))
    marks = np.asarray(marks, float)
    if len(marks) < 4:
        return x.copy()
    target_period = period / ratio
    target_marks = np.arange(marks[1], len(x) - marks[1], target_period)
    out = np.zeros_like(x)
    norm = np.zeros_like(x)
    half = max(16, int(round(cycles * period / 2)))
    for target_mark in target_marks:
        src_index = int(round((target_mark / target_marks[-1]) * (len(marks) - 1)))
        src_index = int(np.clip(src_index, 0, len(marks) - 1))
        source_mark = marks[src_index]
        si, ti = int(round(source_mark)), int(round(target_mark))
        a0, a1 = si - half, si + half + 1
        b0, b1 = ti - half, ti + half + 1
        if min(a0, b0) < 0 or a1 > len(x) or b1 > len(x):
            continue
        window = np.hanning(a1 - a0)
        grain = x[a0:a1].copy()
        if np.any(norm[b0:b1] > 0):
            existing = np.where(
                norm[b0:b1] > 1e-8,
                out[b0:b1] / np.maximum(norm[b0:b1], 1e-8),
                0,
            )
            if np.dot(existing, grain * window) < 0:
                grain *= -1
        out[b0:b1] += grain * window
        norm[b0:b1] += window
    valid = norm > 1e-6
    out[valid] /= norm[valid]
    out[~valid] = x[~valid]
    return out

def phase_locked_vocoder(D, rate, hop_length):
    n_fft = (D.shape[0] - 1) * 2
    phase_advance = 2 * np.pi * hop_length * np.arange(D.shape[0]) / n_fft
    steps = np.arange(0, D.shape[1] - 1, rate)
    out = np.zeros((D.shape[0], len(steps)), complex)
    phase_acc = np.angle(D[:, 0])
    for oi, step in enumerate(steps):
        i = int(np.floor(step))
        frac = step - i
        mag = (1 - frac) * np.abs(D[:, i]) + frac * np.abs(D[:, i + 1])
        p0 = np.angle(D[:, i])
        p1 = np.angle(D[:, i + 1])
        delta = p1 - p0 - phase_advance
        delta -= 2 * np.pi * np.round(delta / (2 * np.pi))
        true_advance = phase_advance + delta
        peaks = np.where(
            (mag[1:-1] > mag[:-2]) & (mag[1:-1] >= mag[2:])
        )[0] + 1
        if len(peaks) == 0:
            phase_acc += true_advance
            out[:, oi] = mag * np.exp(1j * phase_acc)
            continue
        peak_phase = phase_acc[peaks] + true_advance[peaks]
        boundaries = np.r_[0, (peaks[:-1] + peaks[1:]) // 2, len(mag) - 1]
        new_phase = np.empty_like(phase_acc)
        for pi, peak in enumerate(peaks):
            lo, hi = boundaries[pi], boundaries[pi + 1] + 1
            relative = p0[lo:hi] - p0[peak]
            new_phase[lo:hi] = peak_phase[pi] + relative
        phase_acc = new_phase
        out[:, oi] = mag * np.exp(1j * phase_acc)
    return out

def phase_locked_shift(x, semitones, n_fft=2048, hop=256):
    ratio = 2 ** (semitones / 12)
    D = librosa.stft(x, n_fft=n_fft, hop_length=hop, window="hann", center=True)
    stretched = phase_locked_vocoder(D, rate=1 / ratio, hop_length=hop)
    y = librosa.istft(stretched, hop_length=hop, window="hann", length=None)
    return np.asarray(resample(y, len(x)), float)

def harmonicity(x, f0):
    period = int(round(FS / f0))
    if period <= 0 or period >= len(x) // 2:
        return 0.0
    a, b = x[:-period], x[period:]
    return float(np.dot(a, b) / (np.sqrt(np.dot(a, a) * np.dot(b, b)) + 1e-12))

def adaptive_hybrid(x, f0, semitones, noise_level, transient):
    ratio = 2 ** (semitones / 12)
    h = harmonicity(x, f0)
    psola = expanded_psola(x, f0, ratio)
    spectral = phase_locked_shift(x, semitones)
    periodic = np.clip((h - .45) / .45, 0, 1)
    small_shift = np.clip(1 - abs(semitones) / 9.0, 0, 1)
    psola_weight = periodic * small_shift * (.45 if transient else 1.0)
    psola_weight *= (1 - .55 * noise_level)
    psola_weight = float(np.clip(psola_weight, 0, 1))
    y = np.sqrt(psola_weight) * psola + np.sqrt(1 - psola_weight) * spectral
    y /= max(np.max(np.abs(y)), 1e-12) / max(np.max(np.abs(x)), 1e-12)
    return y, psola_weight, h

def spectral_metrics(y, reference, formants):
    n = min(len(y), len(reference))
    y, reference = y[:n], reference[:n]
    Y = np.abs(np.fft.rfft(y * np.hanning(n), n=65536))
    R = np.abs(np.fft.rfft(reference * np.hanning(n), n=65536))
    f = np.fft.rfftfreq(65536, 1 / FS)
    def smooth(a, width=121):
        return np.convolve(a, np.ones(width) / width, mode="same")
    yd = 20 * np.log10(np.maximum(smooth(Y), 1e-10))
    rd = 20 * np.log10(np.maximum(smooth(R), 1e-10))
    mask = (f >= 100) & (f <= 8000)
    diff = ((yd[mask] - np.mean(yd[mask]))
            - (rd[mask] - np.mean(rd[mask])))
    envelope_rmse = float(np.sqrt(np.mean(diff ** 2)))
    peak_errors = []
    for fc, _, _ in formants:
        local = (f >= max(80, fc - 250)) & (f <= fc + 250)
        peak_errors.append(abs(f[local][np.argmax(yd[local])] - fc))
    return envelope_rmse, float(np.mean(peak_errors))

def main():
    cases = [
        ("steady_small", "a", 150., 2, .00, False),
        ("steady_medium", "i", 180., 5, .00, False),
        ("large_shift", "u", 130., 10, .00, False),
        ("breathy_small", "a", 200., 3, .22, False),
        ("breathy_large", "i", 160., 8, .30, False),
        ("transient_medium", "u", 190., 5, .08, True),
    ]
    rows = []
    for case, vowel, f0, semitones, noise, transient in cases:
        formants = VOWELS[vowel]
        source = synth_segment(f0, formants, noise=noise, transient=transient)
        oracle = synth_segment(
            f0 * 2 ** (semitones / 12), formants, noise=noise, transient=transient
        )
        hybrid, weight, h = adaptive_hybrid(
            source, f0, semitones, noise, transient
        )
        methods = {
            "expanded_PSOLA": expanded_psola(
                source, f0, 2 ** (semitones / 12)
            ),
            "phase_locked_spectral": phase_locked_shift(source, semitones),
            "adaptive_hybrid": hybrid,
        }
        for name, y in methods.items():
            envelope, formant = spectral_metrics(y, oracle, formants)
            rows.append({
                "case": case,
                "method": name,
                "semitones": semitones,
                "noise": noise,
                "transient": transient,
                "harmonicity": h if name == "adaptive_hybrid" else np.nan,
                "hybrid_psola_weight": weight if name == "adaptive_hybrid" else np.nan,
                "envelope_rmse_db": envelope,
                "mean_formant_peak_error_hz": formant,
            })
    results = pd.DataFrame(rows)
    results.to_csv(OUT / "hybrid_resynthesis_v1_results.csv", index=False)
    print(results.groupby("method")[
        ["envelope_rmse_db", "mean_formant_peak_error_hz"]
    ].agg(["mean", "median", "max"]).round(3))
    print("\nPASS: synthetic hybrid experiment written")

if __name__ == "__main__":
    main()
