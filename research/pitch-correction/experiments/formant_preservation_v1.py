"""Synthetic source/filter formant-preservation experiment for Monkey's Ear.

Evidence state: SYNTHETIC.

This is not an audio resynthesis benchmark. It isolates one question: when pitch is
shifted, how much spectral-envelope error is introduced if harmonic amplitudes stay
attached to harmonic index (formants travel with pitch) versus being re-evaluated
against the original vocal-tract envelope at the shifted frequencies.
"""
from __future__ import annotations

import csv
from pathlib import Path
import numpy as np

OUT = Path(__file__).with_name("formant_preservation_v1_results.csv")
FORMANTS = [(700.0, 90.0, 1.0), (1200.0, 120.0, 0.72), (2600.0, 180.0, 0.42)]


def envelope(f):
    f = np.asarray(f, dtype=float)
    v = np.full_like(f, 0.02)
    for fc, bw, gain in FORMANTS:
        v += gain / (1.0 + ((f - fc) / (bw / 2.0)) ** 2)
    return v


def run():
    rows = []
    eps = 1e-8
    for f0 in (110.0, 180.0, 260.0):
        for semitones in (-7.0, 5.0, 12.0):
            beta = 2.0 ** (semitones / 12.0)
            h = np.arange(1, int(12000.0 / f0) + 1)
            old_f = h * f0
            new_f = old_f * beta
            mask = new_f < 12000.0
            old_f = old_f[mask]
            new_f = new_f[mask]

            coupled = envelope(old_f)
            preserved = envelope(new_f)
            target = envelope(new_f)

            coupled_rmse = np.sqrt(np.mean((20.0 * np.log10(coupled + eps) - 20.0 * np.log10(target + eps)) ** 2))
            preserved_rmse = np.sqrt(np.mean((20.0 * np.log10(preserved + eps) - 20.0 * np.log10(target + eps)) ** 2))
            rows.append({
                "f0_hz": f0,
                "semitones": semitones,
                "coupled_log_envelope_rmse_db": float(coupled_rmse),
                "preserved_log_envelope_rmse_db": float(preserved_rmse),
                "harmonics": int(len(new_f)),
            })

    with OUT.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    print("f0_hz,semitones,coupled_log_envelope_rmse_db,preserved_log_envelope_rmse_db,harmonics")
    for r in rows:
        print(f"{r['f0_hz']:.1f},{r['semitones']:.1f},{r['coupled_log_envelope_rmse_db']:.6f},{r['preserved_log_envelope_rmse_db']:.6f},{r['harmonics']}")
    print(f"mean_coupled_error_db={np.mean([r['coupled_log_envelope_rmse_db'] for r in rows]):.6f}")


if __name__ == "__main__":
    run()
