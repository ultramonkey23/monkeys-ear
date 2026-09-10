"""Synthetic detector baseline for Monkey's Ear pitch-correction research.

Evidence state: SYNTHETIC. This compares two replaceable F0 estimators only; it does
not validate production pitch correction or vocal quality.
"""
import math
import numpy as np

FS = 48000
FRAME = 2048
HOP = 240
FMIN = 70.0
FMAX = 700.0
MIN_TAU = int(FS / FMAX)
MAX_TAU = int(FS / FMIN)


def parabolic(y, i):
    if i <= 0 or i >= len(y) - 1:
        return float(i)
    a, b, c = y[i - 1], y[i], y[i + 1]
    den = a - 2 * b + c
    return float(i if abs(den) < 1e-12 else i + 0.5 * (a - c) / den)


def yin_f0(x, threshold=0.15):
    d = np.zeros(MAX_TAU + 1)
    for tau in range(1, MAX_TAU + 1):
        z = x[:-tau] - x[tau:]
        d[tau] = np.dot(z, z)
    cmnd = np.ones_like(d)
    acc = 0.0
    for tau in range(1, MAX_TAU + 1):
        acc += d[tau]
        cmnd[tau] = d[tau] * tau / max(acc, 1e-20)
    candidate = None
    for tau in range(MIN_TAU, MAX_TAU):
        if cmnd[tau] < threshold and cmnd[tau] <= cmnd[tau - 1] and cmnd[tau] < cmnd[tau + 1]:
            candidate = tau
            break
    if candidate is None:
        candidate = MIN_TAU + int(np.argmin(cmnd[MIN_TAU:MAX_TAU + 1]))
    refined = parabolic(cmnd, candidate)
    confidence = max(0.0, min(1.0, 1.0 - cmnd[candidate]))
    return FS / refined, confidence


def mpm_f0(x, clarity_threshold=0.8):
    nsdf = np.zeros(MAX_TAU + 1)
    for tau in range(MAX_TAU + 1):
        a, b = x[:len(x) - tau], x[tau:]
        den = np.dot(a, a) + np.dot(b, b)
        nsdf[tau] = 0.0 if den <= 1e-20 else 2.0 * np.dot(a, b) / den
    peaks = [i for i in range(MIN_TAU + 1, MAX_TAU)
             if nsdf[i] > nsdf[i - 1] and nsdf[i] >= nsdf[i + 1] and nsdf[i] > 0.0]
    if not peaks:
        i = MIN_TAU + int(np.argmax(nsdf[MIN_TAU:MAX_TAU + 1]))
        return FS / parabolic(nsdf, i), max(0.0, float(nsdf[i]))
    values = np.array([nsdf[i] for i in peaks])
    cutoff = clarity_threshold * values.max()
    chosen = next((i for i in peaks if nsdf[i] >= cutoff), peaks[int(np.argmax(values))])
    return FS / parabolic(nsdf, chosen), max(0.0, float(nsdf[chosen]))


def synth_case(name, duration=1.5):
    t = np.arange(int(FS * duration)) / FS
    if name == "stable_220":
        f = np.full_like(t, 220.0); noise = 0.0
    elif name == "vibrato":
        f = 220.0 * 2.0 ** ((35.0 * np.sin(2 * np.pi * 5.5 * t)) / 1200.0); noise = 0.0
    elif name == "slide":
        cents = np.clip((t - 0.3) / 0.7, 0.0, 1.0) * 700.0
        f = 220.0 * 2.0 ** (cents / 1200.0); noise = 0.0
    elif name == "strong_2nd_harmonic":
        f = np.full_like(t, 180.0); noise = 0.0
    elif name == "noisy":
        f = np.full_like(t, 250.0); noise = 0.12
    elif name == "breathy":
        f = np.full_like(t, 200.0); noise = 0.28
    else:
        raise ValueError(name)
    phase = 2 * np.pi * np.cumsum(f) / FS
    if name == "strong_2nd_harmonic":
        y = 0.10 * np.sin(phase) + 0.75 * np.sin(2 * phase) + 0.25 * np.sin(3 * phase)
    else:
        y = 0.70 * np.sin(phase) + 0.28 * np.sin(2 * phase) + 0.12 * np.sin(3 * phase)
    if noise:
        rng = np.random.default_rng(1234 + len(name))
        y += noise * rng.standard_normal(len(y))
    env = np.minimum(1.0, t / 0.05) * np.minimum(1.0, (duration - t) / 0.08)
    return y * env, f


def run():
    rows = []
    cases = ["stable_220", "vibrato", "slide", "strong_2nd_harmonic", "noisy", "breathy"]
    for case in cases:
        y, true_f = synth_case(case)
        for algorithm, detector in (("YIN", yin_f0), ("MPM", mpm_f0)):
            errors = []
            octave_errors = 0
            confidences = []
            for start in range(0, len(y) - FRAME, HOP):
                frame_data = y[start:start + FRAME] * np.hanning(FRAME)
                estimate, confidence = detector(frame_data)
                target = true_f[start + FRAME // 2]
                cents = 1200.0 * math.log2(max(estimate, 1e-9) / target)
                errors.append(abs(cents))
                confidences.append(confidence)
                octave_errors += int(abs(cents) > 600.0)
            rows.append((case, algorithm, np.median(errors), np.percentile(errors, 95),
                         octave_errors / len(errors), np.mean(confidences)))
    print("case,algorithm,median_abs_cents,p95_abs_cents,octave_error_rate,mean_confidence")
    for row in rows:
        print(f"{row[0]},{row[1]},{row[2]:.6f},{row[3]:.6f},{row[4]:.6f},{row[5]:.6f}")


if __name__ == "__main__":
    run()
