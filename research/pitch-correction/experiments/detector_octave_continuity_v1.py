"""Candidate-level octave continuity and voiced/unvoiced transitions in detector testing.

Evidence state: SYNTHETIC. Requires: numpy (installed), this directory's
detector_baseline_v1.py (estimators and harmonic corpus).

RESEARCH_HISTORY "Next evidence" item 1: add candidate-level octave
continuity/hysteresis and voiced/unvoiced transitions to detector testing.

Three selection layers are compared on identical per-frame estimator output:

  raw        — single-hypothesis consumption like the product's ExternalSubharmonic
               (tonal.h): the frame's f0 passes straight through with a hard
               voiced gate conf > 0.35 (the product seam at audio_input.cpp:12).
  continuity_strict — octave-register candidates {f0/2, f0, 2*f0} chosen per
               frame: an octave-shifted candidate must beat within-register raw
               tracking by more than OCTAVE_MARGIN_CENTS, plus a hangover V/UV
               state machine (minimum voiced/unvoiced runs) instead of a
               per-frame threshold. Never follows a persistent raw register
               change, so a real octave jump is held off (reported as
               jump-retention error — an informative failure mode).
  continuity_resync — same, plus persistent-disagreement resync: after
               RESYNC_FRAMES consecutive frames where raw disagrees with the
               selection register by more than RESYNC_MARGIN_CENTS, adopt raw
               (real jumps pass; transient slips stay suppressed).

Metrics (ground truth from generation): cents error on truly voiced frames,
spurious octave transitions per second on scenarios with NO ground-truth jump,
voiced/unvoiced frame accuracy, and V/UV switch count. Scenarios with real
octave jumps report jump-retention: the layer must not "continuity away" a
real jump.

This compares detector-selection behavior only. It does not validate production
pitch correction, vocal quality, or usability; those require host execution and
Director-approved listening (TEST_PROTOCOL.md).
"""
import math
import numpy as np

from detector_baseline_v1 import FS, FRAME, HOP, yin_f0, mpm_f0, synth_case

OCTAVE_MARGIN_CENTS = 30.0
RESYNC_MARGIN_CENTS = 300.0    # raw-vs-selection disagreement counted as mismatch
RESYNC_FRAMES = 3              # persistent mismatches before adopting raw
HARD_GATE_CONF = 0.35          # product seam mirror (audio_input.cpp:12)
MIN_VOICED_RUN = 4             # frames of hangover before accepting voiced
MIN_UNVOICED_RUN = 3           # frames of hangover before releasing to unvoiced


def continuity_layer(estimates, confidences, resync_frames=0):
    """Candidate octave selection with hysteresis + hangover V/UV state.

    estimates: raw per-frame f0 (float array); confidences: per-frame 0..1.
    resync_frames: 0 = strict hysteresis (never follow a persistent raw jump);
    N > 0 = resync to the raw estimate after N consecutive frames where raw
    disagrees with the selection by more than RESYNC_MARGIN_CENTS (lets real
    jumps through while keeping slip suppression).
    Returns (selected_f0 array, voiced bool array).
    """
    selected = np.zeros(len(estimates))
    voiced = np.zeros(len(estimates), dtype=bool)
    prev_sel = None
    v_state = False
    v_run = 0
    u_run = 0
    mismatch_run = 0
    for i, (f0, conf) in enumerate(zip(estimates, confidences)):
        # V/UV state machine with hangover (candidate-level transitions).
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
            prev_sel = None   # octave continuity resets across unvoiced gaps
            mismatch_run = 0
            continue
        cands = (-1, 0, 1)   # octave registers: f0/2, f0, 2*f0
        if prev_sel is None:
            chosen = f0          # no history: take the frame estimate
        else:
            costs = {r: abs(1200.0 * math.log2(max(f0 * (2.0 ** r), 1e-9) / prev_sel))
                     for r in cands}
            r_star = min(cands, key=lambda r: costs[r])
            # Octave-REGISTER correction with hysteresis: an octave-shifted
            # candidate must beat tracking raw by more than the margin; within
            # the current register the selection simply tracks raw (no freeze).
            if r_star == 0 or (costs[0] - costs[r_star]) <= OCTAVE_MARGIN_CENTS:
                chosen = f0
            else:
                chosen = f0 * (2.0 ** r_star)
            # Persistent-disagreement resync (when enabled): raw consistently in
            # another register means a real change, not a slip.
            if resync_frames and costs[0] > RESYNC_MARGIN_CENTS:
                mismatch_run += 1
                if mismatch_run >= resync_frames:
                    chosen = f0
                    mismatch_run = 0
            else:
                mismatch_run = 0
        selected[i] = chosen
        prev_sel = chosen
    return selected, voiced


def raw_layer(estimates, confidences):
    """Single-hypothesis consumption with the hard voiced gate (product seam)."""
    voiced = confidences > HARD_GATE_CONF
    selected = np.where(voiced, estimates, 0.0)
    return selected, voiced


def make_case(name):
    """Scenarios with voiced/unvoiced ground truth and optional octave jumps.

    Returns (y, true_f, true_voiced). true_f is 0 on unvoiced samples.
    """
    duration = 1.8
    t = np.arange(int(FS * duration)) / FS
    true_v = np.ones(len(t), dtype=bool)
    if name == "octave_jump":
        f = np.where(t < 0.9, 220.0, 440.0)
    elif name == "vibrato":
        f = 220.0 * 2.0 ** ((35.0 * np.sin(2 * np.pi * 5.5 * t)) / 1200.0)
    elif name == "gap_silence":
        f = np.full_like(t, 220.0)
        true_v = ~((t >= 0.6) & (t < 0.75))
    elif name == "gap_noise":
        f = np.full_like(t, 220.0)
        true_v = ~((t >= 0.6) & (t < 0.75))
    elif name == "strong_2nd_harmonic":
        f = np.full_like(t, 180.0)
    else:
        raise ValueError(name)
    phase = 2 * np.pi * np.cumsum(np.where(true_v, f, 0.0)) / FS
    if name == "strong_2nd_harmonic":
        y = 0.10 * np.sin(phase) + 0.75 * np.sin(2 * phase) + 0.25 * np.sin(3 * phase)
    else:
        y = 0.70 * np.sin(phase) + 0.28 * np.sin(2 * phase) + 0.12 * np.sin(3 * phase)
    rng = np.random.default_rng(1234 + len(name))
    env = np.minimum(1.0, t / 0.05) * np.minimum(1.0, (duration - t) / 0.08)
    y = y * env * np.where(true_v, 1.0, 0.0)   # harmonic energy only while voiced
    if name == "gap_silence":
        pass                                    # unvoiced gap is true silence
    elif name == "gap_noise":
        noise_gate = ((t >= 0.6) & (t < 0.75)).astype(float)
        y = y + 0.28 * noise_gate * rng.standard_normal(len(y))   # fricative-like gap
    else:
        y = y + 0.02 * rng.standard_normal(len(y))
    true_f = np.where(true_v, f, 0.0)
    return y, true_f, true_v


def run():
    cases = ["octave_jump", "vibrato", "gap_silence", "gap_noise", "strong_2nd_harmonic"]
    rows = []
    for case in cases:
        y, true_f, true_v = make_case(case)
        for est_name, detector in (("YIN", yin_f0), ("MPM", mpm_f0)):
            estimates, confidences = [], []
            for start in range(0, len(y) - FRAME, HOP):
                frame_data = y[start:start + FRAME] * np.hanning(FRAME)
                f0, conf = detector(frame_data)
                estimates.append(f0)
                confidences.append(conf)
            estimates = np.array(estimates)
            confidences = np.array(confidences)
            frame_mid = np.arange(len(estimates)) * HOP + FRAME // 2
            gt_f = true_f[frame_mid]
            gt_v = true_v[frame_mid]
            has_jump = case == "octave_jump"
            for layer_name, layer in (
                    ("raw", raw_layer),
                    ("continuity_strict", lambda e, c: continuity_layer(e, c, 0)),
                    ("continuity_resync",
                     lambda e, c: continuity_layer(e, c, RESYNC_FRAMES))):
                sel, v_hat = layer(estimates, confidences)
                voiced_mask = gt_v & v_hat
                if voiced_mask.any():
                    cents = 1200.0 * np.log2(
                        np.maximum(sel[voiced_mask], 1e-9) / gt_f[voiced_mask])
                    med = float(np.median(np.abs(cents)))
                    p95 = float(np.percentile(np.abs(cents), 95))
                else:
                    med = p95 = float("nan")
                # Non-jump cases: spurious octave transitions per voiced second.
                # Jump case: retention error (cents) of post-jump median vs 440 Hz
                # — continuity must not smooth a real jump away.
                slip_rate = float("nan")
                jump_err = float("nan")
                if not has_jump:
                    vs = sel[voiced_mask]
                    if len(vs) > 2:
                        deltas = np.abs(1200.0 * np.log2(np.maximum(vs[1:], 1e-9) /
                                                        np.maximum(vs[:-1], 1e-9)))
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
