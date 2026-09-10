"""Monkey's Ear candidate renderer planner v1.

Evidence state: SYNTHETIC / FAILURE-PRESERVING.

Builds short voiced regions with known formant geometry, generates six
candidates per region:
  PSOLA raw / CEP40 / LPC20
  phase-locked spectral raw / CEP40 / LPC20
and asks a source-referenced planner to choose without access to the oracle.

Important: this experiment intentionally preserves planner failure. The first
planner collapses toward the CEP40 spectral candidate because its score is
dominated by reconstructed-envelope similarity. Oracle diagnostics show that
this can miss PSOLA candidates that preserve formant-peak geometry better.

Requires numpy, scipy, pandas, librosa.
"""
import numpy as np
import pandas as pd
from pathlib import Path
from scipy.signal import resample, freqz
from scipy.linalg import toeplitz
import librosa

FS = 48000
OUT = Path(__file__).resolve().parent
RNG = np.random.default_rng(20260907)

VOWELS = {
    "a": [(730, 90, 1.0), (1090, 110, .72), (2440, 180, .45)],
    "i": [(270, 70, 1.0), (2290, 140, .65), (3010, 180, .42)],
    "u": [(300, 80, 1.0), (870, 100, .72), (2240, 170, .40)],
}

def envelope(f, formants):
    f = np.asarray(f, float)
    y = np.full_like(f, .015)
    for fc, bw, g in formants:
        y += g / (1 + ((f - fc) / (bw / 2)) ** 2)
    return y / np.sqrt(np.maximum(f, 80) / 80)

def synth(f0, formants, dur=.42, noise=0., transient=False):
    t = np.arange(int(FS * dur)) / FS
    h = np.arange(1, int((FS / 2 - 500) // f0) + 1)
    freqs = h * f0
    amps = envelope(freqs, formants)
    phase = (h * .37) % (2 * np.pi)
    y = np.sum(
        amps[:, None] * np.sin(2 * np.pi * freqs[:, None] * t + phase[:, None]),
        axis=0,
    )
    if noise:
        n = RNG.standard_normal(len(y))
        n = np.r_[0, np.diff(n)]
        y += noise * n / max(np.std(n), 1e-12)
    if transient:
        n = int(.012 * FS)
        y[:n] += np.hanning(2 * n)[:n] * RNG.standard_normal(n) * 2
    edge = int(.02 * FS)
    ramp = np.linspace(0, 1, edge)
    y[:edge] *= ramp
    y[-edge:] *= ramp[::-1]
    return y / (max(np.max(np.abs(y)), 1e-12) * 1.02)

def psola(x, f0, ratio):
    period = FS / f0
    marks = np.arange(int(2 * period), len(x) - int(2 * period), period)
    target = np.arange(marks[1], len(x) - marks[1], period / ratio)
    out = np.zeros_like(x)
    norm = np.zeros_like(x)
    half = max(16, int(round(period)))
    for tm in target:
        sm = marks[min(len(marks) - 1, int(round((tm / target[-1]) * (len(marks) - 1))))]
        si, ti = int(round(sm)), int(round(tm))
        a0, a1 = si - half, si + half + 1
        b0, b1 = ti - half, ti + half + 1
        if min(a0, b0) < 0 or a1 > len(x) or b1 > len(x):
            continue
        w = np.hanning(a1 - a0)
        out[b0:b1] += x[a0:a1] * w
        norm[b0:b1] += w
    ok = norm > 1e-6
    out[ok] /= norm[ok]
    out[~ok] = x[~ok]
    return out

def phase_locked_shift(x, semitones, n_fft=2048, hop=256):
    ratio = 2 ** (semitones / 12)
    D = librosa.stft(x, n_fft=n_fft, hop_length=hop, window="hann", center=True)
    advance = 2 * np.pi * hop * np.arange(D.shape[0]) / n_fft
    steps = np.arange(0, D.shape[1] - 1, 1 / ratio)
    out = np.zeros((D.shape[0], len(steps)), complex)
    phase_acc = np.angle(D[:, 0])
    for oi, step in enumerate(steps):
        i = int(step)
        frac = step - i
        mag = (1 - frac) * np.abs(D[:, i]) + frac * np.abs(D[:, i + 1])
        p0, p1 = np.angle(D[:, i]), np.angle(D[:, i + 1])
        delta = p1 - p0 - advance
        delta -= 2 * np.pi * np.round(delta / (2 * np.pi))
        peaks = np.where((mag[1:-1] > mag[:-2]) & (mag[1:-1] >= mag[2:]))[0] + 1
        if not len(peaks):
            phase_acc += advance + delta
        else:
            peak_phase = phase_acc[peaks] + advance[peaks] + delta[peaks]
            bounds = np.r_[0, (peaks[:-1] + peaks[1:]) // 2, len(mag) - 1]
            new_phase = np.empty_like(phase_acc)
            for j, peak in enumerate(peaks):
                lo, hi = bounds[j], bounds[j + 1] + 1
                new_phase[lo:hi] = peak_phase[j] + (p0[lo:hi] - p0[peak])
            phase_acc = new_phase
        out[:, oi] = mag * np.exp(1j * phase_acc)
    stretched = librosa.istft(out, hop_length=hop, window="hann")
    return np.asarray(resample(stretched, len(x)), float)

def cep_env(x, nfft=32768, lifter=40):
    mag = np.maximum(np.abs(np.fft.rfft(x * np.hanning(len(x)), nfft)), 1e-12)
    cep = np.fft.irfft(np.log(mag), nfft)
    kept = np.zeros_like(cep)
    kept[:lifter + 1] = cep[:lifter + 1]
    kept[-lifter:] = cep[-lifter:]
    return np.exp(np.fft.rfft(kept, nfft).real)

def lpc_env(x, nfft=32768, order=20):
    z = x - np.mean(x)
    r = np.correlate(z, z, mode="full")[len(z)-1:len(z)+order]
    coeff = np.linalg.solve(toeplitz(r[:-1]) + 1e-8*np.eye(order), -r[1:])
    _, h = freqz([1.0], np.r_[1.0, coeff], worN=nfft//2+1)
    return np.maximum(np.abs(h), 1e-12)

def reconstruct(candidate, source, method):
    nfft = 32768
    fn = cep_env if method == "cep40" else lpc_env
    gain = np.clip(fn(source, nfft) / np.maximum(fn(candidate, nfft), 1e-12),
                   10**(-12/20), 10**(12/20))
    gain = np.convolve(gain, np.ones(31)/31, mode="same")
    y = np.fft.irfft(np.fft.rfft(candidate, nfft) * gain, nfft)[:len(candidate)]
    y *= np.sqrt(np.mean(candidate**2)+1e-12) / np.sqrt(np.mean(y**2)+1e-12)
    return y

def harmonicity(x, f0):
    p = int(round(FS / f0))
    a, b = x[:-p], x[p:]
    return float(np.dot(a, b) / (np.sqrt(np.dot(a,a)*np.dot(b,b)) + 1e-12))

def env_distance(a, b):
    ea, eb = cep_env(a), cep_env(b)
    f = np.fft.rfftfreq(32768, 1/FS)
    m = (f >= 100) & (f <= 8000)
    d = 20*np.log10(np.maximum(ea[m],1e-12)) - 20*np.log10(np.maximum(eb[m],1e-12))
    d -= np.mean(d)
    return float(np.sqrt(np.mean(d*d)))

def planner_score(source, candidate, target_f0):
    return env_distance(candidate, source) + 3*max(0, .55-harmonicity(candidate,target_f0))

def oracle_metrics(y, oracle, formants):
    env = env_distance(y, oracle)
    nfft = 65536
    f = np.fft.rfftfreq(nfft, 1/FS)
    db = 20*np.log10(np.maximum(np.abs(np.fft.rfft(y*np.hanning(len(y)), nfft)),1e-10))
    errors = []
    for fc, _, _ in formants:
        m = (f >= max(80, fc-250)) & (f <= fc+250)
        errors.append(abs(f[m][np.argmax(db[m])] - fc))
    return env, float(np.mean(errors))

def main():
    cases = [
        ("clean_small","a",150.,2,.00,False),
        ("clean_mid","i",180.,5,.00,False),
        ("clean_large","u",130.,10,.00,False),
        ("breathy_small","a",200.,3,.22,False),
        ("breathy_large","i",160.,8,.30,False),
        ("transient_mid","u",190.,5,.08,True),
        ("transient_large","a",140.,9,.10,True),
        ("high_f0","i",300.,4,.02,False),
    ]
    rows = []
    for case, vowel, f0, semitones, noise, transient in cases:
        formants = VOWELS[vowel]
        source = synth(f0, formants, noise=noise, transient=transient)
        target_f0 = f0 * 2**(semitones/12)
        oracle = synth(target_f0, formants, noise=noise, transient=transient)
        raw = {
            "PSOLA": psola(source, f0, 2**(semitones/12)),
            "SPECTRAL": phase_locked_shift(source, semitones),
        }
        candidates = {}
        for family, y in raw.items():
            candidates[family+"_raw"] = y
            candidates[family+"_cep40"] = reconstruct(y, source, "cep40")
            candidates[family+"_lpc20"] = reconstruct(y, source, "lpc20")
        scores = {k: planner_score(source, y, target_f0) for k, y in candidates.items()}
        selected = min(scores, key=scores.get)
        diagnostics = {k: oracle_metrics(y, oracle, formants) for k,y in candidates.items()}
        oracle_best = min(diagnostics, key=lambda k: diagnostics[k][0] + .03*diagnostics[k][1])
        for name, y in candidates.items():
            env, formant = diagnostics[name]
            rows.append({
                "case": case, "candidate": name, "planner_score": scores[name],
                "selected": name == selected, "oracle_best": name == oracle_best,
                "oracle_envelope_rmse_db": env,
                "oracle_formant_peak_error_hz": formant,
            })
    df = pd.DataFrame(rows)
    df.to_csv(OUT/"candidate_planner_v1_results.csv", index=False)
    selected = df[df.selected]
    print(selected[["case","candidate","oracle_envelope_rmse_db",
                    "oracle_formant_peak_error_hz","oracle_best"]].to_string(index=False))
    print("oracle-best hit rate", float(selected.oracle_best.mean()))
    print("PASS: failure-preserving planner experiment written")

if __name__ == "__main__":
    main()
