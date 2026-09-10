"""Synthetic contour-decomposition experiment for Monkey's Ear vocal pitch correction.

Evidence state: SYNTHETIC.

Goal: test whether separating note center, slow drift, rapid modulation/vibrato,
and note-transition trajectory can outperform whole-contour snapping/pulling on a
controlled pitch contour. This does not transform audio and is not listening proof.
"""
from __future__ import annotations

import csv
from pathlib import Path
import numpy as np

HOP_S = 0.005
SEEDS = range(1000, 1200)
OUT = Path(__file__).with_name("contour_decomposition_v1_results.csv")


def moving_average(x: np.ndarray, n: int) -> np.ndarray:
    n = max(3, int(n) | 1)
    pad = n // 2
    xp = np.pad(x, (pad, pad), mode="edge")
    return np.convolve(xp, np.ones(n) / n, mode="valid")


def make_phrase(seed: int):
    r = np.random.default_rng(seed)
    t = np.arange(0.0, 3.0, HOP_S)
    note_targets = np.array([
        0.0,
        float(r.choice([200.0, 300.0, 400.0, 500.0, 700.0])),
        float(r.choice([-100.0, 0.0, 100.0, 200.0, 300.0])),
    ])
    stepped = np.select([t < 1.0, t < 2.0], [note_targets[0], note_targets[1]], default=note_targets[2])
    target_curve = stepped.copy()
    transition_mask = np.zeros_like(t, dtype=bool)
    for k, boundary in enumerate((1.0, 2.0)):
        duration = float(r.uniform(0.10, 0.24))
        pre = duration * 0.45
        post = duration - pre
        mask = (t >= boundary - pre) & (t <= boundary + post)
        transition_mask |= mask
        u = (t[mask] - (boundary - pre)) / duration
        s = u * u * (3.0 - 2.0 * u)
        target_curve[mask] = note_targets[k] + (note_targets[k + 1] - note_targets[k]) * s

    centers = np.select(
        [t < 1.0, t < 2.0],
        [r.uniform(-45.0, 45.0), r.uniform(-45.0, 45.0)],
        default=r.uniform(-45.0, 45.0),
    )
    drift = np.zeros_like(t)
    for start, end in ((0.0, 1.0), (1.0, 2.0), (2.0, 3.0)):
        mask = (t >= start) & (t < end)
        u = (t[mask] - start) / (end - start)
        slope = r.uniform(-30.0, 30.0)
        curve = r.uniform(-12.0, 12.0) * (2.0 * u - 1.0) ** 2
        drift[mask] = slope * (u - 0.5) + curve - np.mean(curve)

    rate = r.uniform(4.5, 7.0)
    depths = np.array([r.uniform(18.0, 45.0), r.uniform(18.0, 45.0), r.uniform(18.0, 45.0)])
    depth = np.select([t < 1.0, t < 2.0], [depths[0], depths[1]], default=depths[2])
    vibrato = depth * np.sin(2.0 * np.pi * rate * t + r.uniform(0.0, 2.0 * np.pi))
    observed = target_curve + centers + drift + vibrato
    return t, stepped, target_curve, observed, drift, vibrato, transition_mask


def component_correct(t: np.ndarray, target_curve: np.ndarray, observed: np.ndarray) -> np.ndarray:
    """Estimate slow/fast residual components from contour + chosen target trajectory.

    Requested behavior for this experiment:
      center correction = 100%
      drift correction = 70%
      vibrato reduction = 5% (95% retained)
    """
    residual = observed - target_curve
    slow = moving_average(residual, int(0.205 / HOP_S))
    fast = residual - slow
    note_idx = np.minimum((t // 1.0).astype(int), 2)
    center = np.zeros_like(t)
    for k in range(3):
        stable = (note_idx == k) & ((t - k) >= 0.16) & ((t - k) <= 0.84)
        c = np.median(slow[stable])
        center[note_idx == k] = c
    drift = slow - center
    return target_curve + 0.30 * drift + 0.95 * fast


def evaluate(t, target_curve, y, drift, vibrato, transition_mask):
    ideal = target_curve + 0.30 * drift + 0.95 * vibrato
    stable = ~transition_mask
    fast = y - target_curve - moving_average(y - target_curve, int(0.205 / HOP_S))
    return {
        "center_abs_mean_cents": float(abs(np.mean((y - target_curve)[stable]))),
        "rmse_vs_requested_component_mix": float(np.sqrt(np.mean((y - ideal) ** 2))),
        "transition_rmse_vs_target_curve": float(np.sqrt(np.mean((y[transition_mask] - target_curve[transition_mask]) ** 2))),
        "vibrato_fast_rms_retention": float(np.std(fast[stable]) / max(np.std(vibrato[stable]), 1e-9)),
    }


def run():
    rows = []
    for seed in SEEDS:
        t, stepped, target_curve, observed, drift, vibrato, transition_mask = make_phrase(seed)
        strategies = {
            "hard_snap": stepped,
            "whole_contour_75pct": observed + 0.75 * (stepped - observed),
            "component_estimator_v1": component_correct(t, target_curve, observed),
        }
        for name, y in strategies.items():
            rows.append({"seed": seed, "strategy": name, **evaluate(t, target_curve, y, drift, vibrato, transition_mask)})

    with OUT.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    print("strategy,mean_center_abs_cents,mean_rmse,mean_transition_rmse,mean_vibrato_retention,max_rmse")
    for strategy in ("component_estimator_v1", "hard_snap", "whole_contour_75pct"):
        subset = [r for r in rows if r["strategy"] == strategy]
        print(
            f"{strategy},"
            f"{np.mean([r['center_abs_mean_cents'] for r in subset]):.6f},"
            f"{np.mean([r['rmse_vs_requested_component_mix'] for r in subset]):.6f},"
            f"{np.mean([r['transition_rmse_vs_target_curve'] for r in subset]):.6f},"
            f"{np.mean([r['vibrato_fast_rms_retention'] for r in subset]):.6f},"
            f"{np.max([r['rmse_vs_requested_component_mix'] for r in subset]):.6f}"
        )


if __name__ == "__main__":
    run()
