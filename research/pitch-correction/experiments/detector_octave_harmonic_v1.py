"""Harmonic-evidence octave disambiguation in detector testing.

Evidence state: SYNTHETIC. Requires: numpy (installed), this directory's
detector_baseline_v1.py (estimators) and detector_octave_continuity_v1.py
(layer machinery and scenarios).

RESEARCH_HISTORY "Next evidence" item 1 remainder: use harmonic/energy
evidence (not just temporal continuity) to disambiguate octave candidates.

Problem this addresses (measured in detector_octave_continuity_v1): on
strong_2nd_harmonic (f0 = 180 Hz, weak fundamental 0.10 vs 2nd harmonic
0.75) the MPM estimator locks to 360 Hz with a persistent ~1200 cents bias
that survives both continuity layers, because register-scoped hysteresis and
resync both consume the estimator's own hypothesis. Temporal evidence cannot
correct a persistent raw register error; a different evidence source is
required.

Hypothesis: a per-frame harmonic evidence test — the LOWEST octave candidate
whose own fundamental is visibly present in the frame spectrum claims the
octave — disambiguates the register without hurting correct registers.

Evidence rule (deterministic, no tuning to the outcome): for the raw frame
spectrum, a candidate f_c is "visible" when the spectral peak near f_c
exceeds VIS_FLOOR of the highest peak in the evidence band. Among registers
{f_raw/2, f_raw, 2*f_raw}, the lowest visible register is chosen; if none is
visible the raw register is kept. The same hangover V/UV state machine and
persistent-disagreement resync from detector_octave_continuity_v1 are applied.

Layers compared on identical per-frame estimator output:

  raw              — single-hypothesis consumption (product seam).
  continuity_resync — best temporal layer from the prior experiment.
  harmonic_strict  — harmonic-evidence register choice, no resync.
  harmonic_resync  — harmonic-evidence register choice + resync.

Metrics are identical to detector_octave_continuity_v1 for direct comparison.

This compares detector-selection behavior on synthetic frames only. It does
not validate production pitch correction, vocal quality, or usability; those
require host execution and Director-approved listening (TEST_PROTOCOL.md).
"""
import math
import numpy as np

from detector_baseline_v1 import FS, FRAME, HOP, yin_f0, mpm_f0
from detector_octave_continuity_v1 import (
    HARD_GATE_CONF,
    continuity_layer,
    MIN_VOICED_RUN,
    MIN_UNVOICED_RUN,
    OCTAVE_MARGIN_CENTS,
    RESYNC_MARGIN_CENTS,
    RESYNC_FRAMES,
    make_case,
    raw_layer,
)

VIS_FLOOR = 0.05          # peak visibility floor, relative to highest band peak
EVIDENCE_BIN_TOL = 2      # bins (resolution = FS/FRAME = 23.4 Hz)
EVIDENCE_BAND_HZ = (80.0, 900.0)


def frame_peaks(frame):
    """Magnitudes and peak-height query for one windowed frame."""
    spec = np.abs(np.fft.rfft(frame))
    freqs = np.fft.rfftfreq(len(frame), 1.0 / FS)
    lo = np.searchsorted(freqs, EVIDENCE_BAND_HZ[0])
    hi = np.searchsorted(freqs, EVIDENCE_BAND_HZ[1])
    band_max = float(np.max(spec[lo:hi])) if hi > lo else 0.0

    def peak_at(f):
        if f <= 0.0 or band_max <= 0.0:
            return 0.0
        center = int(round(f * len(frame) / FS))
        a = max(0, center - EVIDENCE_BIN_TOL)
        b = min(len(spec), center + EVIDENCE_BIN_TOL + 1)
        return float(np.max(spec[a:b]))

    return band_max, peak_at


def evidence_register(frame, f_raw):
    """Lowest octave candidate whose own fundamental is visibly present.

    Returns r in (-1, 0, 1). No visible fundamental keeps the raw register.
    """
    if f_raw <= 0.0:
        return 0
    band_max, peak_at = frame_peaks(frame)
    for r in (-1, 0, 1):
        f = f_raw * (2.0 ** r)
        if peak_at(f) > VIS_FLOOR * band_max:
            return r
    return 0


def harmonic_layer(estimates, confidences, frames, resync_frames=0):
    """Octave-register selection by harmonic evidence + hangover V/UV state.

    Same structure as detector_octave_continuity_v1.continuity_layer, but the
    register is claimed by the frame's harmonic evidence rather than by
    temporal tracking costs. Hysteresis (OCTAVE_MARGIN_CENTS) still governs
    register changes; resync (when enabled) adopts raw after persistent raw
    disagreement so a real jump is not held forever.
    """
    selected = np.zeros(len(estimates))
    voiced = np.zeros(len(estimates), dtype=bool)
    prev_sel = None
    v_state = False
    v_run = 0
    u_run = 0
    mismatch_run = 0
    for i, (f0, conf) in enumerate(zip(estimates, confidences)):
        if conf > HARD_GATE_CONF:
            v_run += 1
            u_run = 0
        else:
            u_run += 1
            v_run = 0
        if not v_state and v_run >= MIN_VOICED_RUN:
            v_state = True
        elif v_state and u_run >= MIN_UNVOICED_RUN:
            v_state = False
        voiced[i] = v_state
        if not v_state:
            selected[i] = 0.0
            prev_sel = None
            mismatch_run = 0
            continue
        r_ev = evidence_register(frames[i], f0)
        f_ev = f0 * (2.0 ** r_ev)
        if prev_sel is None:
            chosen = f_ev
        else:
            cost_raw = abs(1200.0 * math.log2(max(f0, 1e-9) / prev_sel))
            cost_ev = abs(1200.0 * math.log2(max(f_ev, 1e-9) / prev_sel))
            # Hysteresis: follow the evidence register only when it is
            # meaningfully better than raw tracking; otherwise keep tracking
            # raw inside the current register (one-frame evidence errors
            # cannot flip the octave).
            if (cost_raw - cost_ev) > OCTAVE_MARGIN_CENTS:
                chosen = f_ev
            elif (cost_ev - cost_raw) > OCTAVE_MARGIN_CENTS:
                chosen = f0
            else:
                chosen = f_ev if cost_ev <= cost_raw else f0
            # Persistent raw-vs-selection disagreement lets a real jump pass
            # when raw (not evidence) is the persistent outlier.
            if resync_frames and cost_raw > RESYNC_MARGIN_CENTS \
                    and cost_ev > RESYNC_MARGIN_CENTS:
                mismatch_run += 1
                if mismatch_run >= resync_frames:
                    chosen = f0
                    mismatch_run = 0
            else:
                mismatch_run = 0
        selected[i] = chosen
        prev_sel = chosen
    return selected, voiced


def run():
    cases = ["octave_jump", "vibrato", "gap_silence", "gap_noise",
             "strong_2nd_harmonic"]
    rows = []
    for case in cases:
        y, true_f, true_v = make_case(case)
        for est_name, detector in (("YIN", yin_f0), ("MPM", mpm_f0)):
            estimates, confidences, frames = [], [], []
            for start in range(0, len(y) - FRAME, HOP):
                frame_data = y[start:start + FRAME] * np.hanning(FRAME)
                f0, conf = detector(frame_data)
                estimates.append(f0)
                confidences.append(conf)
                frames.append(frame_data)
            estimates = np.array(estimates)
            confidences = np.array(confidences)
            frame_mid = np.arange(len(estimates)) * HOP + FRAME // 2
            gt_f = true_f[frame_mid]
            gt_v = true_v[frame_mid]
            has_jump = case == "octave_jump"
            layer_specs = (
                ("raw", lambda e, c, f: raw_layer(e, c)),
                ("continuity_resync",
                 lambda e, c, f: continuity_layer(e, c, RESYNC_FRAMES)),
                ("harmonic_strict",
                 lambda e, c, f: harmonic_layer(e, c, f, 0)),
                ("harmonic_resync",
                 lambda e, c, f: harmonic_layer(e, c, f, RESYNC_FRAMES)),
            )
            for layer_name, layer in layer_specs:
                sel, v_hat = layer(estimates, confidences, frames)
                voiced_mask = gt_v & v_hat
                if voiced_mask.any():
                    cents = 1200.0 * np.log2(
                        np.maximum(sel[voiced_mask], 1e-9) / gt_f[voiced_mask])
                    med = float(np.median(np.abs(cents)))
                    p95 = float(np.percentile(np.abs(cents), 95))
                else:
                    med = p95 = float("nan")
                slip_rate = float("nan")
                jump_err = float("nan")
                if not has_jump:
                    vs = sel[voiced_mask]
                    if len(vs) > 2:
                        deltas = np.abs(1200.0 * np.log2(
                            np.maximum(vs[1:], 1e-9) / np.maximum(vs[:-1], 1e-9)))
                        slips = int(np.sum(deltas > 600.0))
                        voiced_s = int(np.sum(voiced_mask)) / (FS / HOP)
                        slip_rate = slips / max(voiced_s, 1e-9)
                else:
                    after = gt_v & v_hat & (frame_mid > 0.95 * FS)
                    if after.any():
                        ret = float(np.median(sel[after]))
                        jump_err = abs(1200.0 * math.log2(max(ret, 1e-9) / 440.0))
                vuv_acc = float(np.mean(v_hat == gt_v))
                vuv_switches = int(np.sum(v_hat[1:] != v_hat[:-1]))
                rows.append((case, est_name, layer_name, med, p95, slip_rate,
                             jump_err, vuv_acc, vuv_switches))
    print("case,estimator,layer,median_abs_cents,p95_abs_cents,"
          "octave_slip_rate_per_s,jump_retention_error_cents,"
          "vuv_frame_accuracy,vuv_switch_count")
    for r in rows:
        print(f"{r[0]},{r[1]},{r[2]},{r[3]:.6f},{r[4]:.6f},{r[5]:.6f},{r[6]:.6f},"
              f"{r[7]:.6f},{r[8]}")


if __name__ == "__main__":
    run()
