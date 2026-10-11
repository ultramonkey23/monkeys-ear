"""Persistence-gated evidence register in detector testing.

Evidence state: SYNTHETIC. Requires: numpy (installed), this directory's
detector_baseline_v1.py (estimators) and detector_octave_continuity_v1.py
(layer machinery and scenarios).

RESEARCH_HISTORY "Next evidence" item 1 remainder: onset re-entry slip
control, with the persistence-gated evidence register (adopt an evidence
register only after N consecutive agreeing frames) as the named candidate.

Problem this addresses (measured in detector_octave_harmonic_v1): the
harmonic-evidence register repairs the persistent MPM strong-2nd-harmonic
bias (1202.9c -> 2.89c) but costs onset re-entry robustness — single frames
after a silence/noise gap briefly mislead the visibility rule, producing
~1.25 register slips/s on gap episodes where continuity_resync had 0.

Hypothesis: gating evidence adoption on persistence (the evidence register
must claim the same register for N consecutive voiced frames before the
selection adopts it) removes single-frame onset flips while keeping the
persistent-bias repair, because a persistent bias keeps agreeing with its
evidence register and is adopted after N frames.

Mechanism (deterministic, declared before the run; no tuning to the outcome):
the per-frame evidence rule is unchanged from detector_octave_harmonic_v1
(lowest octave candidate whose fundamental is visibly present; VIS_FLOOR).
New: the evidence claim only updates a pending register with a run counter;
the adopted register changes only after PERSIST_FRAMES consecutive identical
claims. V/UV release resets adoption state so a new episode must re-prove
its register. Selection then tracks the estimator hypothesis inside the
adopted register (f0 * 2^adopted), so the adoption gate is the ONLY octave
change authority.

Negative result preserved (first mechanism, measured then corrected): the
first version kept detector_octave_harmonic_v1's per-frame cost competition
between raw and evidence as a second gate. It removed the onset slips but
NEVER repaired the persistent bias (1202.89c remained), because the adopted
register change was a 1200-cent move from the previous selection and the
hysteresis rule (cost_ev - cost_raw > OCTAVE_MARGIN_CENTS -> keep raw)
vetoed exactly the persistent repair the gate had just approved. In the
harmonic layer the bias repair had come from the un-gated first voiced frame
(prev_sel=None -> follow evidence), the same path that produced the onset
flips. Lesson: one authority per decision — once persistence proves a
register claim, a second one-frame hysteresis must not veto it. The v1
mechanism is kept as layer persistence_cost3_resync so the correction is
visible in the numbers, not just in prose.

PERSIST_FRAMES primary = 3 (a priori, matching the RESYNC_FRAMES convention;
~15 ms at HOP=240/FS=48000). Sensitivity rows for N=2 and N=5 are reported
as sensitivity, not as a selection sweep.

Layers compared on identical per-frame estimator output:

  raw                  — single-hypothesis consumption (product seam).
  continuity_resync    — best temporal layer from detector_octave_continuity_v1.
  harmonic_resync      — best evidence layer from detector_octave_harmonic_v1.
  persistence_cost3_resync — v1 (cost competition + gate), preserved negative.
  persistence2         — v2 adoption-gated register tracking (sensitivity).
  persistence3         — v2, primary gate.
  persistence5         — v2 (sensitivity).

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
from detector_octave_harmonic_v1 import evidence_register, harmonic_layer

PERSIST_FRAMES_PRIMARY = 3   # declared primary gate (a priori)
PERSIST_SENSITIVITY = (2, 5)


def _persistence_adoption(estimates, confidences, frames, persist_frames):
    """Yield (f0, adopted_r or None) per frame under the persistence gate.

    The evidence claim (unchanged harmonic rule) only updates a pending
    register with a run counter; adoption happens after `persist_frames`
    consecutive identical claims. Unvoiced frames reset the state.
    """
    if persist_frames < 1:
        raise ValueError("persist_frames must be >= 1")
    v_state = False
    v_run = 0
    u_run = 0
    pending_r = None
    pending_run = 0
    adopted_r = None
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
        if not v_state:
            pending_r = None
            pending_run = 0
            adopted_r = None
            yield f0, None, False
            continue
        r_ev = evidence_register(frames[i], f0)
        if r_ev == pending_r:
            pending_run += 1
        else:
            pending_r = r_ev
            pending_run = 1
        if pending_run >= persist_frames:
            adopted_r = pending_r
        yield f0, adopted_r, True


def persistence_layer(estimates, confidences, frames, persist_frames):
    """v2: adoption-gated register tracking.

    Selection tracks the estimator hypothesis inside the adopted register
    (f0 * 2^adopted); before adoption (including every onset re-entry) the
    raw register is used. The persistence gate is the only octave-change
    authority — no per-frame cost competition (see module docstring for the
    measured v1 negative that motivated this).
    """
    selected = np.zeros(len(estimates))
    voiced = np.zeros(len(estimates), dtype=bool)
    for i, (f0, adopted_r, is_voiced) in enumerate(
            _persistence_adoption(estimates, confidences, frames,
                                  persist_frames)):
        voiced[i] = is_voiced
        if not is_voiced:
            continue
        r_use = 0 if adopted_r is None else adopted_r
        selected[i] = f0 * (2.0 ** r_use)
    return selected, voiced


def persistence_cost_layer(estimates, confidences, frames, persist_frames,
                           resync_frames=0):
    """v1 preserved negative: persistence gate + harmonic-layer cost competition.

    Kept only so the correction's effect is measurable; do not promote.
    """
    selected = np.zeros(len(estimates))
    voiced = np.zeros(len(estimates), dtype=bool)
    prev_sel = None
    mismatch_run = 0
    for i, (f0, adopted_r, is_voiced) in enumerate(
            _persistence_adoption(estimates, confidences, frames,
                                  persist_frames)):
        voiced[i] = is_voiced
        if not is_voiced:
            selected[i] = 0.0
            prev_sel = None
            mismatch_run = 0
            continue
        r_use = 0 if adopted_r is None else adopted_r
        f_ev = f0 * (2.0 ** r_use)
        if prev_sel is None:
            chosen = f_ev
        else:
            cost_raw = abs(1200.0 * math.log2(max(f0, 1e-9) / prev_sel))
            cost_ev = abs(1200.0 * math.log2(max(f_ev, 1e-9) / prev_sel))
            if (cost_raw - cost_ev) > OCTAVE_MARGIN_CENTS:
                chosen = f_ev
            elif (cost_ev - cost_raw) > OCTAVE_MARGIN_CENTS:
                chosen = f0
            else:
                chosen = f_ev if cost_ev <= cost_raw else f0
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
                ("harmonic_resync",
                 lambda e, c, f: harmonic_layer(e, c, f, RESYNC_FRAMES)),
                ("persistence_cost3_resync",
                 lambda e, c, f: persistence_cost_layer(
                     e, c, f, PERSIST_FRAMES_PRIMARY, RESYNC_FRAMES)),
                ("persistence2",
                 lambda e, c, f: persistence_layer(
                     e, c, f, PERSIST_SENSITIVITY[0])),
                ("persistence3",
                 lambda e, c, f: persistence_layer(
                     e, c, f, PERSIST_FRAMES_PRIMARY)),
                ("persistence5",
                 lambda e, c, f: persistence_layer(
                     e, c, f, PERSIST_SENSITIVITY[1])),
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
