"""V7 subtle auto-dynamics guardrail simulation.

Research-only control-rate model. This is not production DSP or perceptual validation.
It tests the product rule that automatic EQ dynamics stay subtle unless a user
explicitly enables stronger behavior.
"""

import numpy as np
import pandas as pd

FS = 200.0
DT = 1.0 / FS
AUTO_MAX_GR_DB = 1.5
AUTO_ATTACK_MS = 90.0
AUTO_RELEASE_MS = 260.0
TRANSIENT_HOLD_MS = 30.0
AUTO_SLEW_DB_PER_S = 8.0


def onepole(prev, target, tau_ms):
    a = 1.0 - np.exp(-DT / (tau_ms / 1000.0))
    return prev + a * (target - prev)


def bounded_auto(severity, transient=None, enabled=None):
    severity = np.clip(np.asarray(severity, dtype=float), 0.0, 1.0)
    n = len(severity)
    transient = np.zeros(n) if transient is None else np.clip(np.asarray(transient, dtype=float), 0.0, 1.0)
    enabled = np.ones(n, dtype=bool) if enabled is None else np.asarray(enabled, dtype=bool)
    y = np.zeros(n)
    persistence = 0.0
    hold = 0
    max_step = AUTO_SLEW_DB_PER_S * DT

    for i in range(1, n):
        persistence = onepole(persistence, severity[i], 120.0)
        if transient[i] > 0.6:
            hold = max(hold, int(TRANSIENT_HOLD_MS / 1000.0 * FS))
        protect = 0.15 if hold > 0 else 1.0
        if hold > 0:
            hold -= 1
        desired = -AUTO_MAX_GR_DB * (persistence ** 1.35) * protect if enabled[i] else 0.0
        tau = AUTO_ATTACK_MS if desired < y[i - 1] else AUTO_RELEASE_MS
        candidate = onepole(y[i - 1], desired, tau)
        y[i] = y[i - 1] + np.clip(candidate - y[i - 1], -max_step, max_step)
    return y


def naive_dynamic(severity):
    return -4.0 * np.clip(np.asarray(severity, dtype=float), 0.0, 1.0)


def scenario(kind, dur=4.0):
    t = np.arange(int(dur * FS)) / FS
    s = np.zeros_like(t)
    tr = np.zeros_like(t)
    en = np.ones_like(t, dtype=bool)
    if kind == "isolated_transient":
        s[(t >= 1.0) & (t < 1.04)] = 1.0
        tr[(t >= 1.0) & (t < 1.04)] = 1.0
    elif kind == "sustained_conflict":
        s[(t >= 1.0) & (t < 3.0)] = 0.85
    elif kind == "repeated_hits":
        for start in np.arange(0.5, 3.6, 0.35):
            m = (t >= start) & (t < start + 0.055)
            s[m] = 0.9
            tr[m] = 1.0
    elif kind == "level_step":
        s[(t >= 0.7) & (t < 1.7)] = 0.35
        s[(t >= 1.7) & (t < 3.2)] = 0.95
    elif kind == "priority_disabled":
        s[(t >= 0.7) & (t < 3.4)] = 0.9
        en[t >= 2.0] = False
    return t, s, tr, en


def run():
    rows = []
    for kind in ["isolated_transient", "sustained_conflict", "repeated_hits", "level_step", "priority_disabled", "silence"]:
        t, s, tr, en = scenario(kind)
        auto = bounded_auto(s, tr, en)
        naive = naive_dynamic(s)
        rows.append({
            "scenario": kind,
            "auto_peak_GR_dB": round(float(abs(auto.min())), 3),
            "naive_peak_GR_dB": round(float(abs(naive.min())), 3),
            "auto_mean_abs_GR_dB": round(float(np.mean(abs(auto))), 3),
            "auto_chatter_dB_per_s": round(float(np.sum(abs(np.diff(auto))) / (t[-1] - t[0])), 3),
            "naive_chatter_dB_per_s": round(float(np.sum(abs(np.diff(naive))) / (t[-1] - t[0])), 3),
            "auto_transient_damage_dB": None if not np.any(tr > 0) else round(float(abs(auto[tr > 0]).max()), 3),
            "naive_transient_damage_dB": None if not np.any(tr > 0) else round(float(abs(naive[tr > 0]).max()), 3),
            "final_auto_GR_dB": round(float(auto[-1]), 4),
        })

    df = pd.DataFrame(rows)
    print(df.to_string(index=False))
    assert df["auto_peak_GR_dB"].max() <= 1.501
    assert df.loc[df.scenario == "isolated_transient", "auto_transient_damage_dB"].iloc[0] < 0.15
    assert abs(df.loc[df.scenario == "silence", "final_auto_GR_dB"].iloc[0]) < 1e-6
    assert abs(df.loc[df.scenario == "priority_disabled", "final_auto_GR_dB"].iloc[0]) < 0.02
    return df


if __name__ == "__main__":
    run()
