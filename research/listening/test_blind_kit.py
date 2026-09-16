#!/usr/bin/env python3
"""Checks for blind_kit.py. Run with: python test_blind_kit.py"""

import json
import math
import os
import struct
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blind_kit as kit  # noqa: E402


def write_pcm(path, samples, rate, bits):
    """Independent PCM writer so the parser is not only tested against itself."""
    samples = np.atleast_2d(np.asarray(samples, dtype=np.float64).T).T
    channels = samples.shape[1]
    scale = 2 ** (bits - 1)
    ints = np.clip(np.round(samples * scale), -scale, scale - 1).astype(np.int64)
    if bits == 16:
        body = ints.astype("<i2").tobytes()
    elif bits == 24:
        u = (ints.reshape(-1) & 0xFFFFFF).astype(np.uint32)
        body = np.stack([u & 0xFF, (u >> 8) & 0xFF, (u >> 16) & 0xFF], axis=1).astype(np.uint8).tobytes()
    else:
        body = ints.astype("<i4").tobytes()
    align = channels * bits // 8
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * align, align, bits)
    # A LIST chunk before data exercises chunk skipping and odd-size padding.
    junk = b"LIST" + struct.pack("<I", 3) + b"abc\x00"
    riff = b"WAVE" + b"fmt " + struct.pack("<I", 16) + fmt + junk + b"data" + struct.pack("<I", len(body)) + body
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(riff)) + riff)


def sine(freq, rate, seconds, amp=1.0, phase=0.0):
    t = np.arange(int(rate * seconds)) / rate
    return amp * np.sin(2 * math.pi * freq * t + phase)


def test_bs1770_reference():
    rate = 48000
    left = sine(997, rate, 5.0)
    stereo = np.stack([left, np.zeros_like(left)], axis=1)
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "ref.wav")
        kit.write_wav_float32(path, stereo, rate)
        x, r, _ = kit.read_wav(path)
    lufs = kit.integrated_loudness(x, r)
    assert abs(lufs - (-3.01)) <= 0.1, f"reference read {lufs:.3f} LUFS"
    # 48 kHz coefficients must equal the ones printed in BS.1770.
    (b, a), (_, ha) = kit.k_weighting_coefficients(48000)
    ref = [1.53512485958697, -2.69169618940638, 1.19839281085285,
           -1.69065929318241, 0.73248077421585, -1.99004745483398, 0.99007225036621]
    got = [*b, a[1], a[2], ha[1], ha[2]]
    assert all(abs(g - e) < 1e-6 for g, e in zip(got, ref)), got


def test_rate_independence():
    # The same tone at 44.1 kHz must read the same loudness as at 48 kHz.
    readings = [kit.integrated_loudness(sine(997, r, 3.0, 0.5)[:, None], r) for r in (44100, 48000)]
    assert abs(readings[0] - readings[1]) < 0.05, readings


def test_true_peak_intersample():
    # fs/4 sine at 45 degrees: samples sit at 0.7071 but the waveform reaches 1.0.
    x = sine(12000, 48000, 1.0, phase=math.pi / 4)[:, None]
    sample_peak = 20 * math.log10(np.max(np.abs(x)))
    tp = kit.true_peak_dbtp(x)
    assert abs(sample_peak + 3.01) < 0.01, sample_peak
    assert abs(tp) < 0.1, tp


def test_pcm_parsing():
    x = sine(440, 44100, 0.1, 0.5)
    with tempfile.TemporaryDirectory() as d:
        for bits in (16, 24, 32):
            path = os.path.join(d, f"p{bits}.wav")
            write_pcm(path, np.stack([x, -x], axis=1), 44100, bits)
            y, r, name = kit.read_wav(path)
            assert r == 44100 and y.shape == (len(x), 2) and name == f"pcm{bits}"
            assert np.max(np.abs(y[:, 0] - x)) < 2.0 / 2 ** (bits - 1), bits
            assert np.max(np.abs(y[:, 1] + x)) < 2.0 / 2 ** (bits - 1), bits


def make_renders(d):
    rng = np.random.default_rng(7)
    paths = []
    specs = [  # (name, rate, bits, stereo, amp)
        ("dry_render.wav", 44100, 16, False, 0.2),
        ("monkeys_ear_render.wav", 48000, 24, True, 0.6),
        ("melodyne_render.wav", 48000, 32, True, 0.05),
    ]
    for name, rate, bits, stereo, amp in specs:
        tone = sine(220, rate, 2.0, amp) + amp * 0.1 * rng.standard_normal(int(rate * 2.0))
        tone = np.stack([tone, 0.8 * tone], axis=1) if stereo else tone[:, None]
        path = os.path.join(d, name)
        write_pcm(path, tone, rate, bits)
        paths.append(path)
    # One float32 input near full scale.
    rate = 48000
    tone = sine(330, rate, 2.0, 0.9)
    path = os.path.join(d, "float_render.wav")
    kit.write_wav_float32(path, np.stack([tone, tone], axis=1), rate)
    paths.append(path)
    return paths


def test_kit_end_to_end():
    with tempfile.TemporaryDirectory() as d:
        paths = make_renders(d)
        hashes = {p: kit.sha256_file(p) for p in paths}
        out = os.path.join(d, "kit")
        manifest = kit.build_kit(paths, out, seed=1234)

        # Originals untouched.
        for p in paths:
            assert kit.sha256_file(p) == hashes[p], p

        # Matched outputs agree within 0.1 LU (re-measured independently here).
        assert not manifest["clamped_labels"]
        measured = []
        for m in manifest["matched"]:
            fpath = os.path.join(out, m["file"])
            assert kit.sha256_file(fpath) == m["sha256"]
            x, r, name = kit.read_wav(fpath)
            assert name == "float32"
            measured.append(kit.integrated_loudness(x, r))
        assert max(measured) - min(measured) <= 0.1, measured
        assert abs(min(measured) - manifest["target_lufs"]) <= 0.1

        # Every original hash is recorded; labels are opaque.
        assert sorted(o["sha256"] for o in manifest["originals"]) == sorted(hashes.values())
        assert [m["label"] for m in manifest["matched"]] == ["A", "B", "C", "D"]

        # Key is separate and absent from the manifest.
        with open(os.path.join(out, "manifest.json"), encoding="utf-8") as f:
            text = f.read()
        with open(os.path.join(out, "key.json"), encoding="utf-8") as f:
            key = json.load(f)["key"]
        assert set(key) == {"A", "B", "C", "D"}
        assert sorted(v["source"] for v in key.values()) == sorted(os.path.abspath(p) for p in paths)
        for p in paths:
            stem = os.path.splitext(os.path.basename(p))[0]
            assert stem not in text, f"{stem} leaked into manifest"
        assert '"source"' not in text and '"key"' not in text
        assert all(v["source"].replace("\\", "\\\\") not in text for v in key.values())

        # Same seed and file set -> same key regardless of argument order.
        out2 = os.path.join(d, "kit2")
        kit.build_kit(list(reversed(paths)), out2, seed=1234)
        with open(os.path.join(out2, "key.json"), encoding="utf-8") as f:
            assert json.load(f)["key"] == key

        # Refuses to write into a non-empty directory (e.g. the renders' folder).
        try:
            kit.build_kit(paths, d, seed=1)
        except kit.KitError:
            pass
        else:
            raise AssertionError("wrote into a non-empty directory")
        for p in paths:
            assert kit.sha256_file(p) == hashes[p], p


def test_boost_clamp_reported():
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "hot.wav")
        write_pcm(path, sine(997, 48000, 1.0, 0.5)[:, None], 48000, 24)
        manifest = kit.build_kit([path], os.path.join(d, "kit"), seed=0, target_lufs=0.0)
        m = manifest["matched"][0]
        assert m["clamped"] and manifest["clamped_labels"] == ["A"]
        assert m["gain_db"] < m["requested_gain_db"]
        assert m["dbtp_after"] <= -1.0 + 0.01, m


def test_plan_gain():
    assert kit.plan_gain(-10.0, 0.5, -20.0, -1.0) == (-10.0, -10.0, False)
    assert kit.plan_gain(-20.0, -3.0, -10.0, -1.0) == (2.0, 10.0, True)
    assert kit.plan_gain(-20.0, 0.0, -10.0, -1.0) == (0.0, 10.0, True)


def main():
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_") and callable(f)]
    failed = 0
    for name, fn in tests:
        try:
            fn()
            print(f"PASS {name}")
        except Exception as exc:  # report every failure, not just the first
            failed += 1
            print(f"FAIL {name}: {exc!r}")
    print(f"{len(tests) - failed}/{len(tests)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
