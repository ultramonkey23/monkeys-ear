#!/usr/bin/env python3
"""Checks that README.md documents blind_kit.py as it really behaves.

Run with: python research/listening/test_readme_contract.py
"""

import contextlib
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import blind_kit as kit  # noqa: E402

README = os.path.join(HERE, "README.md")
PROTOCOL = os.path.join(HERE, "..", "TEST_PROTOCOL.md")


def cli_flags():
    """Long options the real argparse parser accepts, read from --help."""
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        try:
            kit.main(["--help"])
        except SystemExit:
            pass
    return set(re.findall(r"--[a-z][a-z0-9-]*", out.getvalue()))


def readme_problems(text, flags):
    """Return a list of ways `text` misdescribes blind_kit.py (empty if none)."""
    problems = []
    lower = text.lower()

    def need(pattern, why):
        if not re.search(pattern, text, re.IGNORECASE):
            problems.append(why)

    need(r"monkey.s ear.only", "must say it is a Monkey's Ear-only research tool")
    need(r"TEST_PROTOCOL\.md", "must cite research/TEST_PROTOCOL.md")
    need(r"110\s*([-‐-—]|to)\s*116", "must cite TEST_PROTOCOL.md lines 110-116")
    need(r"automatable half", "must say it implements the automatable half")
    need(r"random", "must describe randomized labels")
    need(r"opaque", "must describe opaque labels")
    need(r"BS\.?\s?1770", "must name BS.1770 loudness matching")
    need(r"[-−]1(\.0)?\s*dBTP", "must state the -1 dBTP ceiling")
    need(r"clamp", "must describe the clamp report (clamped / clamped_labels)")
    need(r"unmatched|untouched|never (modif|touch)", "must say unmatched originals are kept")
    need(r"manifest\.json", "must describe manifest.json")
    need(r"key\.json", "must describe key.json")
    need(r"python research/listening/test_blind_kit\.py", "must give the test command")
    need(r"conformance", "must disclaim ITU conformance")
    need(r"cody", "must leave judgment to Cody")
    need(r"preference", "must name preference as human judgment")
    need(r"musical usefulness", "must name musical usefulness as human judgment")

    # blind_kit.py writes key.json separately from manifest.json.
    if re.search(r"no separate\W+key\.json|key\.json\W+is not (created|written)", lower):
        problems.append("wrongly claims key.json is not written")
    # The ceiling clamps boosts; nothing clips and no limiter runs.
    if re.search(r"report\w*\s+(any\s+)?clipping", lower):
        problems.append("describes the clamp report as a clipping report")
    # Every documented long option must exist on the real parser.
    for flag in sorted(set(re.findall(r"(?<![\w-])--[a-z][a-z0-9-]*", text)) - flags):
        problems.append(f"documents {flag}, which blind_kit.py does not accept")
    if not re.search(r"(?<![\w-])--out(?![\w-])", text):  # not --output
        problems.append("must document the required --out option")
    # Unexpanded shell leftovers, e.g. "$(date)".
    if "$(" in text:
        problems.append("contains an unexpanded shell substitution")
    return problems


def test_cli_flags_introspected():
    flags = cli_flags()
    assert {"--out", "--seed", "--target-lufs", "--ceiling-dbtp"} <= flags, flags


def test_default_ceiling_matches_readme_claim():
    assert kit.DEFAULT_CEILING_DBTP == -1.0


def test_protocol_anchor_lines():
    with open(PROTOCOL, encoding="utf-8") as f:
        lines = f.read().splitlines()[109:116]
    joined = "\n".join(lines)
    assert "randomize render names and loudness-match" in joined, joined
    assert "## Level matching" in joined, joined
    assert "unmatched production render" in joined, joined


def test_checker_rejects_invented_usage():
    bad = ("Monkey's Ear-only. TEST_PROTOCOL.md lines 110-116.\n"
           "python research/listening/blind_kit.py --input x --output y --verbose\n"
           "Produces a single manifest.json; no separate `key.json` is created.\n"
           "Reports any clipping. *Generated on $(date)*\n")
    problems = readme_problems(bad, cli_flags())
    text = "\n".join(problems)
    for expected in ("--input", "--output", "--verbose", "key.json is not written",
                     "clipping", "shell substitution", "--out option"):
        assert expected in text, (expected, problems)


def test_readme_matches_tool():
    assert os.path.isfile(README), "research/listening/README.md is missing"
    with open(README, encoding="utf-8") as f:
        problems = readme_problems(f.read(), cli_flags())
    assert not problems, problems


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
