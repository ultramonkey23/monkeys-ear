# Voice/Vocal validation bundle

This repository produces a tester-facing Windows x64 bundle for the standalone
`monkeys_ear_vocal.vst3` effect after the full Windows validation job passes.

The bundle is deliberately narrower than a release. It contains:

- the exact VST3 binary built from the named commit;
- install and removal steps for REAPER testing;
- a machine-readable SHA-256 for the binary and a companion hash for the ZIP;
- the confirmed automation evidence and the explicit human-proof ceiling;
- the minimum context required for a useful failure report.

The workflow verifies the ZIP by extracting it, checking its exact five-file
contract, and recomputing the plug-in checksum before upload.

## What this changes

A successful build becomes usable by a tester without guessing which binary to
copy, where to put it, what the automation actually proved, or which commit a
failure belongs to.

## What this does not change

This is not a public release, license decision, compatibility claim, listening
result, REAPER result, support promise, or permanent download. GitHub Actions
artifacts currently expire after 14 days. A stable distribution path remains a
separate release decision after real host, recall, render, CPU, and listening
proof.
