#!/usr/bin/env python3
"""Offline blind-listening kit for Monkey's Ear renders.

Implements the automatable half of research/TEST_PROTOCOL.md "Blind comparison"
and "Level matching": randomize render names and loudness-match before
listening, while never touching the unmatched originals.

    python blind_kit.py render1.wav render2.wav ... --out kit_dir [--seed N]
                        [--target-lufs L] [--ceiling-dbtp -1.0]

Writes to kit_dir:
    A.wav, B.wav, ...  loudness-matched 32-bit float copies (opaque labels)
    manifest.json      sha256 of originals and matched files, LUFS/dBTP before
                       and after, applied gain, seed (no label->source pairing)
    key.json           label -> source path; keep closed until ratings exist

Dependencies: Python stdlib + numpy. Measurements follow the ITU-R BS.1770
method but this tool makes no conformance claim (see README.md).
"""

import argparse
import datetime
import hashlib
import json
import math
import os
import random
import secrets
import struct
import sys

import numpy as np

ABS_GATE_LUFS = -70.0
REL_GATE_LU = -10.0
BLOCK_S = 0.4
STEP_S = 0.1  # 75% overlap
OVERSAMPLE = 4
DEFAULT_CEILING_DBTP = -1.0

_FMT_PCM = 0x0001
_FMT_FLOAT = 0x0003
_FMT_EXTENSIBLE = 0xFFFE


class KitError(Exception):
    """Raised for unsupported input or unsafe output conditions."""


# --------------------------------------------------------------------------
# RIFF WAV I/O
# --------------------------------------------------------------------------

def read_wav(path):
    """Parse a RIFF WAV file.

    Returns (samples, sample_rate, format_name) where samples is a float64
    array shaped (frames, channels) scaled to +/-1.0 full scale.
    Supports PCM 16/24/32-bit and IEEE float 32/64-bit, plain or extensible.
    """
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 12 or data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise KitError(f"{path}: not a RIFF/WAVE file")

    fmt = None
    payload = None
    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = body
        elif cid == b"data":
            payload = body
        pos += 8 + size + (size & 1)  # chunks are word-aligned
    if fmt is None or len(fmt) < 16:
        raise KitError(f"{path}: missing or short fmt chunk")
    if payload is None:
        raise KitError(f"{path}: missing data chunk")

    tag, channels, rate, _, block_align, bits = struct.unpack_from("<HHIIHH", fmt, 0)
    if tag == _FMT_EXTENSIBLE:
        if len(fmt) < 40:
            raise KitError(f"{path}: truncated WAVE_FORMAT_EXTENSIBLE header")
        tag = struct.unpack_from("<H", fmt, 24)[0]  # first field of SubFormat GUID
    if channels not in (1, 2):
        raise KitError(f"{path}: {channels} channels unsupported (mono/stereo only)")
    if rate <= 0:
        raise KitError(f"{path}: invalid sample rate {rate}")
    if block_align != channels * (bits // 8):
        raise KitError(f"{path}: inconsistent block align {block_align}")

    usable = len(payload) - len(payload) % block_align
    raw = payload[:usable]
    if tag == _FMT_PCM and bits == 16:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
        name = "pcm16"
    elif tag == _FMT_PCM and bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v >= 0x800000, v - 0x1000000, v)
        x = v.astype(np.float64) / 8388608.0
        name = "pcm24"
    elif tag == _FMT_PCM and bits == 32:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
        name = "pcm32"
    elif tag == _FMT_FLOAT and bits == 32:
        x = np.frombuffer(raw, dtype="<f4").astype(np.float64)
        name = "float32"
    elif tag == _FMT_FLOAT and bits == 64:
        x = np.frombuffer(raw, dtype="<f8").astype(np.float64)
        name = "float64"
    else:
        raise KitError(f"{path}: unsupported format tag {tag:#06x} at {bits} bits")
    if not np.all(np.isfinite(x)):
        raise KitError(f"{path}: contains NaN or infinite samples")
    return x.reshape(-1, channels), rate, name


def write_wav_float32(path, samples, rate):
    """Write (frames, channels) samples as IEEE float32 WAV. Refuses to overwrite."""
    samples = np.asarray(samples, dtype=np.float64)
    if samples.ndim == 1:
        samples = samples[:, None]
    frames, channels = samples.shape
    body = samples.astype("<f4").tobytes()
    block_align = channels * 4
    fmt = struct.pack("<HHIIHHH", _FMT_FLOAT, channels, rate,
                      rate * block_align, block_align, 32, 0)
    fact = struct.pack("<I", frames)
    chunks = (b"fmt " + struct.pack("<I", len(fmt)) + fmt
              + b"fact" + struct.pack("<I", len(fact)) + fact
              + b"data" + struct.pack("<I", len(body)) + body
              + (b"\x00" if len(body) & 1 else b""))
    with open(path, "xb") as f:  # "x": never clobber an existing file
        f.write(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def sha256_file(path):
    """Hex sha256 of a file's bytes."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


# --------------------------------------------------------------------------
# BS.1770 measurement
# --------------------------------------------------------------------------

def k_weighting_coefficients(rate):
    """K-weighting pre-filter (high shelf) and RLB high-pass biquads for `rate`.

    Derived from the analog prototypes behind the 48 kHz coefficients printed
    in BS.1770, so that 44.1 kHz (or any rate) gets its own recomputed filters.
    Returns ((b, a) shelf, (b, a) highpass) with a = (1, a1, a2).
    """
    # Stage 1: pre-filter (head-effect high shelf).
    f0, gain_db, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / rate)
    vh = 10.0 ** (gain_db / 20.0)
    vb = vh ** 0.4996667741545416
    a0 = 1.0 + k / q + k * k
    shelf = ((
        (vh + vb * k / q + k * k) / a0,
        2.0 * (k * k - vh) / a0,
        (vh - vb * k / q + k * k) / a0,
    ), (1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0))

    # Stage 2: RLB high-pass.
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / rate)
    a0 = 1.0 + k / q + k * k
    highpass = ((1.0, -2.0, 1.0),
                (1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0))
    return shelf, highpass


def _k_filter_channel(x, shelf, highpass):
    """Run both K-weighting biquads (transposed direct form II) over one channel."""
    (s0, s1, s2), (_, sa1, sa2) = shelf
    (h0, h1, h2), (_, ha1, ha2) = highpass
    out = [0.0] * len(x)
    p1 = p2 = q1 = q2 = 0.0
    i = 0
    for xi in x.tolist():
        y = s0 * xi + p1
        p1 = s1 * xi - sa1 * y + p2
        p2 = s2 * xi - sa2 * y
        z = h0 * y + q1
        q1 = h1 * y - ha1 * z + q2
        q2 = h2 * y - ha2 * z
        out[i] = z
        i += 1
    return np.asarray(out, dtype=np.float64)


def integrated_loudness(samples, rate):
    """Gated integrated loudness in LUFS (-inf if no block survives gating).

    Mono and stereo only, so every channel weight G_i is 1.0.
    """
    shelf, highpass = k_weighting_coefficients(rate)
    block = int(round(BLOCK_S * rate))
    step = int(round(STEP_S * rate))
    frames = samples.shape[0]
    if frames < block:
        return float("-inf")
    starts = np.arange(0, frames - block + 1, step)

    power = np.zeros(len(starts))
    for ch in range(samples.shape[1]):
        y = _k_filter_channel(samples[:, ch], shelf, highpass)
        cs = np.concatenate(([0.0], np.cumsum(y * y)))
        power += (cs[starts + block] - cs[starts]) / block

    with np.errstate(divide="ignore"):
        block_lufs = -0.691 + 10.0 * np.log10(power)
    gated = power[block_lufs > ABS_GATE_LUFS]
    if gated.size == 0:
        return float("-inf")
    rel_gate = -0.691 + 10.0 * math.log10(gated.mean()) + REL_GATE_LU
    final = power[(block_lufs > ABS_GATE_LUFS) & (block_lufs > rel_gate)]
    if final.size == 0:
        return float("-inf")
    return -0.691 + 10.0 * math.log10(final.mean())


def _interpolator(factor=OVERSAMPLE, half_taps_per_side=12, beta=5.0):
    """Kaiser-windowed sinc low-pass for zero-stuffed `factor`x oversampling."""
    n = np.arange(-half_taps_per_side * factor, half_taps_per_side * factor + 1)
    return np.sinc(n / factor) * np.kaiser(len(n), beta)


def true_peak_dbtp(samples):
    """4x-oversampled true peak in dBTP (never below the sample peak)."""
    h = _interpolator()
    peak = float(np.max(np.abs(samples))) if samples.size else 0.0
    for ch in range(samples.shape[1]):
        up = np.zeros(samples.shape[0] * OVERSAMPLE)
        up[::OVERSAMPLE] = samples[:, ch]
        peak = max(peak, float(np.max(np.abs(np.convolve(up, h)))))
    return 20.0 * math.log10(peak) if peak > 0.0 else float("-inf")


def measure(samples, rate):
    """Return (integrated LUFS, true peak dBTP)."""
    return integrated_loudness(samples, rate), true_peak_dbtp(samples)


# --------------------------------------------------------------------------
# Kit assembly
# --------------------------------------------------------------------------

def label_for(index):
    """0 -> A, 25 -> Z, 26 -> AA (bijective base 26)."""
    label = ""
    index += 1
    while index:
        index, rem = divmod(index - 1, 26)
        label = chr(ord("A") + rem) + label
    return label


def plan_gain(lufs, dbtp, target, ceiling):
    """Gain toward `target`; boosts may never push true peak above `ceiling`.

    Returns (applied_gain_db, requested_gain_db, clamped). Attenuation is
    never limited because it cannot raise peaks.
    """
    requested = target - lufs
    applied = requested
    if requested > 0.0 and dbtp + requested > ceiling:
        applied = max(0.0, ceiling - dbtp)
    return applied, requested, applied < requested


def _num(value, digits=4):
    """JSON-safe rounding; non-finite measurements become null."""
    return round(value, digits) if math.isfinite(value) else None


def build_kit(inputs, out_dir, seed=None, target_lufs=None,
              ceiling_dbtp=DEFAULT_CEILING_DBTP):
    """Measure, shuffle, loudness-match and write the kit. Returns the manifest dict."""
    if not inputs:
        raise KitError("no input files")
    sources = [os.path.abspath(p) for p in inputs]
    if len(set(os.path.normcase(p) for p in sources)) != len(sources):
        raise KitError("the same input file was given more than once")
    out_dir = os.path.abspath(out_dir)
    if os.path.exists(out_dir) and os.listdir(out_dir):
        raise KitError(f"{out_dir}: output directory must be new or empty")

    before = []
    for path in sources:
        digest = sha256_file(path)
        samples, rate, fmt_name = read_wav(path)
        lufs, dbtp = measure(samples, rate)
        if not math.isfinite(lufs):
            raise KitError(f"{path}: no gated loudness (silent or shorter than 400 ms)")
        before.append({"source": path, "sha256": digest, "rate": rate,
                       "channels": samples.shape[1], "format": fmt_name,
                       "lufs": lufs, "dbtp": dbtp})

    target_source = "user"
    if target_lufs is None:
        target_lufs = min(item["lufs"] for item in before)
        target_source = "quietest_input"

    if seed is None:
        seed = secrets.randbelow(2 ** 32)
    # Shuffle a canonical order so the key depends only on the file set and seed.
    order = sorted(range(len(before)), key=lambda i: (before[i]["sha256"], before[i]["source"]))
    random.Random(seed).shuffle(order)

    os.makedirs(out_dir, exist_ok=True)
    inputs_norm = {os.path.normcase(p) for p in sources}
    matched, key = [], {}
    for slot, idx in enumerate(order):
        item = before[idx]
        label = label_for(slot)
        out_path = os.path.join(out_dir, f"{label}.wav")
        if os.path.normcase(out_path) in inputs_norm:
            raise KitError(f"{out_path}: output would overwrite an input")
        gain_db, requested_db, clamped = plan_gain(item["lufs"], item["dbtp"],
                                                   target_lufs, ceiling_dbtp)
        samples, rate, _ = read_wav(item["source"])
        write_wav_float32(out_path, samples * 10.0 ** (gain_db / 20.0), rate)

        written, written_rate, _ = read_wav(out_path)  # measure what was actually written
        lufs_after, dbtp_after = measure(written, written_rate)
        matched.append({
            "label": label,
            "file": f"{label}.wav",
            "sha256": sha256_file(out_path),
            "lufs_before": _num(item["lufs"]),
            "dbtp_before": _num(item["dbtp"]),
            "requested_gain_db": _num(requested_db),
            "gain_db": _num(gain_db),
            "clamped": clamped,
            "lufs_after": _num(lufs_after),
            "dbtp_after": _num(dbtp_after),
        })
        key[label] = {"source": item["source"], "sha256": item["sha256"]}

    changed = [b["source"] for b in before if sha256_file(b["source"]) != b["sha256"]]
    if changed:
        raise KitError(f"originals changed during run: {changed}")

    afters = [m["lufs_after"] for m in matched if not m["clamped"]]
    manifest = {
        "tool": "monkeys-ear research/listening/blind_kit.py",
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "seed": seed,
        "target_lufs": _num(target_lufs),
        "target_source": target_source,
        "ceiling_dbtp": ceiling_dbtp,
        "method": {
            "loudness": "BS.1770-style integrated loudness: K-weighting recomputed per rate, "
                        "400 ms blocks, 75% overlap, -70 LUFS absolute and -10 LU relative gates",
            "true_peak": f"{OVERSAMPLE}x oversampled, Kaiser-windowed sinc interpolation",
            "conformance_claim": None,
        },
        # Sorted by hash, without paths: the pairing lives only in key.json.
        "originals": sorted(({
            "sha256": b["sha256"], "format": b["format"], "sample_rate": b["rate"],
            "channels": b["channels"], "lufs": _num(b["lufs"]), "dbtp": _num(b["dbtp"]),
        } for b in before), key=lambda o: o["sha256"]),
        "originals_unchanged": True,
        "matched": matched,
        "clamped_labels": [m["label"] for m in matched if m["clamped"]],
        "matched_lufs_spread_lu": _num(max(afters) - min(afters)) if afters else None,
    }
    with open(os.path.join(out_dir, "manifest.json"), "x", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, allow_nan=False)
        f.write("\n")
    with open(os.path.join(out_dir, "key.json"), "x", encoding="utf-8") as f:
        json.dump({
            "warning": "Do not open until ratings are recorded.",
            "seed": seed,
            "key": key,
        }, f, indent=2, allow_nan=False)
        f.write("\n")
    return manifest


def main(argv=None):
    """CLI entry point."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("inputs", nargs="+", help="WAV renders to compare")
    parser.add_argument("--out", required=True, help="new or empty output directory")
    parser.add_argument("--seed", type=int, help="shuffle seed (default: random, recorded)")
    parser.add_argument("--target-lufs", type=float,
                        help="match target (default: quietest input)")
    parser.add_argument("--ceiling-dbtp", type=float, default=DEFAULT_CEILING_DBTP,
                        help="boosts never exceed this true peak (default: -1.0)")
    args = parser.parse_args(argv)
    try:
        manifest = build_kit(args.inputs, args.out, args.seed,
                             args.target_lufs, args.ceiling_dbtp)
    except (KitError, OSError) as exc:
        print(f"blind_kit: {exc}", file=sys.stderr)
        return 1
    labels = ", ".join(m["file"] for m in manifest["matched"])
    print(f"wrote {labels} to {os.path.abspath(args.out)}")
    print(f"target {manifest['target_lufs']} LUFS ({manifest['target_source']}), "
          f"spread {manifest['matched_lufs_spread_lu']} LU")
    for label in manifest["clamped_labels"]:
        print(f"WARNING: {label} clamped at {args.ceiling_dbtp} dBTP; "
              f"it is quieter than the target", file=sys.stderr)
    print("key.json holds the identities; keep it closed until ratings are recorded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
