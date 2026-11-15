# Preset signal integrity and output offset

LAB_RECEIPT_CONTRACT_V1:
RESULT: PARTIAL
PROJECT_ID: monkeys-ear
MISSION: Investigate Cody's preset buzz and improve the shared audio path, testing and Lab learning from the work.
FILES_CHANGED: include/monkeys_ear/engine.h, src/engine.cpp, test/test_dsp_core.cpp, test/test_vst3_host_probe.cpp, README.md, receipts/receipt_20261114_preset_integrity.md
START_HEAD: fb4bb5062edef9f45b68f3dc5d21993857ea373f
END_HEAD: pending-commit
COMMIT: pending-commit
VALIDATION_RESULT: PARTIAL
RECOMMENDED_NEXT_ACTION: Reproduce Cody's buzzing with named presets and exact MIDI/audio setup, compare level-matched renders and measure aliasing; prove generated and physical MIDI through a single REAPER instance; continue independent modules and connected ecosystem.

## DIRECTOR_READABLE
Cody reported buzzing and strange behavior on some presets and requested broad upgrades including adaptive Lab improvements. The shared synth/FX engine output now rejects DC using a causal 5 Hz stereo filter before the limiter. Existing FERAL WOBBLE 48 kHz render one-second means changed from .0451/.0402/.0379 to .00085/.00082/-.00104; VELVET LEAD changed from .0234/.0291/.0263 to .00094/.00002/-.00038. This change affects low-frequency response; music listening is still required. Neither these measurements nor passing tests prove the reported buzz is fixed.

## AI_DRIFT_LOCK
Do not report the requested broad product work complete: named buzzing presets, aliasing, REAPER MIDI/audio playability, connected module behavior, and listening remain open. Separate Vocal plugin does not use this shared engine output path. DC filtering is not an aliasing solution. Prior MIDI report is Lab feedback IE-d90372f6d057; buzzing report IE-956892c2b16e.

## TECHNICAL_RECEIPT
The full Windows run_tests.bat battery passed with all eight factory palette selections, three sample rates and low/mid/high MIDI notes, initial/reset silence and finite bounded output. A matched two-instance VST3 gain timing comparison replaced an assertion that wrongly demanded a 90 percent gain drop within 64 samples despite the 12 ms smoother. The main DSP battery now keeps assertions enabled under Release; separately compiled and ran CMAKE_BUILD_TYPE=Release core tests. Sample rates 44.1/48/96 kHz produced FERAL WOBBLE sustained stereo DC means within 0.0011. Output DC filter is 5 Hz, per-channel, reset with engine, no new audio callback allocation or reported latency. CPU deadline under this exact integration, nonlinear alias energy, sound preference, and hardware/REAPER playback are not verified. The current instrument binary was copied to both Windows VST3 paths; existing REAPER processes can retain older binaries until reopened.

The connected Lab receipt owner fix was independently executed and committed as b068889c: a previously false DEVICE_PROVEN derivation for the prior REAPER receipt was corrected on re-ingestion; original outcome was already partial. This demonstrates changed ingestion output, not later routing adaptation.
