# Offline blind-listening kit

`blind_kit.py` is a **Monkey's Ear-only research tool**. It prepares renders
for blind listening tests offline. It is not part of the plugin, is not built
into the VST3, and does not touch `src/`, `include/`, `vst3/` or any real-time
code.

## What it covers

It implements the automatable half of `research/TEST_PROTOCOL.md` lines 110-116
("Blind comparison" and "Level matching"), the parts a script can do:

- **Randomized opaque labels.** The inputs are sorted into a fixed order
  (by sha256, then path) and shuffled with a seeded `random.Random`. The
  outputs are named `A.wav`, `B.wav`, ... (after `Z` comes `AA`). The same file
  set and seed always give the same key, whatever order the files are listed
  in. If you leave out `--seed`, the script picks a random one and records it.
- **BS.1770-style loudness matching.** The script measures gated integrated
  loudness with:
  - K-weighting recomputed for each sample rate
  - 400 ms blocks with 75% overlap
  - an absolute gate at -70 LUFS and a relative gate at -10 LU

  It measures true peak with 4x oversampling (Kaiser-windowed sinc). By
  default the target is the quietest input, or you can set it with
  `--target-lufs`.
- **-1 dBTP ceiling and clamp report.** Turning a file down is never limited.
  A boost stops before the true peak would go above `--ceiling-dbtp` (default
  `-1.0`). A file that gets clamped this way is left quieter than the target,
  and the script reports it in three places:
  - `clamped: true` in that file's `manifest.json` entry, next to
    `requested_gain_db` and the `gain_db` it actually applied
  - the `clamped_labels` list
  - a `WARNING` line on stderr

  Clamped files are left out of `matched_lufs_spread_lu`.
- **Unmatched originals are kept.** The script only reads the input renders.
  It hashes them before and after the run and stops with an error if any of
  them changed. Output files are opened in exclusive-create mode, and the
  output directory must be new or empty. If level change is part of the
  intended effect, keep using the original production render next to the
  matched set (protocol line 116).
- **`manifest.json` and a separate `key.json`.**
  - `manifest.json` holds:
    - the seed, target and ceiling
    - a method description
    - the sha256, format, rate, channel count, LUFS and dBTP of each original,
      sorted by hash, with no paths
    - for each label: gain, clamp flag, and LUFS/dBTP before and after
  - `key.json` holds the only mapping from labels to source files. Keep it
    closed until ratings are recorded, so no one learns which render is
    Monkey's Ear (protocol line 110).

## Usage

You need Python 3 with `numpy`. Everything else is from the standard library.
Inputs must be mono or stereo RIFF WAV files: 16-, 24- or 32-bit PCM, or 32- or
64-bit float, plain or `WAVE_FORMAT_EXTENSIBLE`. Each file must be at least
400 ms long and not silent.

```
python research/listening/blind_kit.py render1.wav render2.wav ... --out kit_dir
        [--seed N] [--target-lufs L] [--ceiling-dbtp -1.0]
```

| Argument | Meaning |
| --- | --- |
| `inputs` | WAV renders to compare (one or more, no duplicates) |
| `--out` | New or empty output directory (required) |
| `--seed` | Shuffle seed; if omitted, a random one is used and recorded |
| `--target-lufs` | Loudness to match to; if omitted, the quietest input is used |
| `--ceiling-dbtp` | Highest true peak a boost may reach (default `-1.0`) |

The script writes these files to `kit_dir`:

- `A.wav`, `B.wav`, ...: loudness-matched 32-bit float copies
- `manifest.json`
- `key.json`

It exits with status 1 and prints a `blind_kit:` message if an input is
unsupported, the output directory is not empty, or an original changed during
the run.

## Tests

```
python research/listening/test_blind_kit.py
```

The script prints `PASS`/`FAIL` for each check and ends with an `N/M passed`
line. It exits non-zero if any check fails. The checks cover:

- the BS.1770 48 kHz reference coefficients and a -3.01 LUFS reference tone
- matching loudness readings at 44.1 kHz and 48 kHz
- inter-sample true peak
- parsing of 16-, 24- and 32-bit PCM files
- a full run: originals untouched, matched outputs within 0.1 LU, no source
  names or paths in `manifest.json`, the same key for the same seed, and
  refusal to write into a non-empty directory
- the boost clamp and how it is reported
- `plan_gain`

## Limits

- **No ITU conformance claim.** The measurements follow the ITU-R BS.1770
  method, but the tool has not been checked against the ITU conformance
  material. `manifest.json` records `"conformance_claim": null`. Treat the
  numbers as research measurements, not certified loudness values.
- **The listening decision stays human.** The kit only removes the most obvious
  biases: file names and loudness differences. Preference (which render is
  better) and musical usefulness remain Cody's human judgment. A
  single positive listen is LISTENED evidence, not VERIFIED evidence
  (protocol line 112).
