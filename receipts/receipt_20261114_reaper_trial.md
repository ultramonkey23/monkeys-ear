# Monkey's Ear — REAPER trial and Sound Space silence

LAB_RECEIPT_CONTRACT_V1:
RESULT: PARTIAL
PROJECT_ID: monkeys-ear
MISSION: Address Cody's report of shallow UX, disconnected plugin workflows, and MIDI failures; use Lab evidence owners.
FILES_CHANGED: include/monkeys_ear/sound_space.h, src/sound_space.cpp, test/test_dsp_core.cpp, test/test_vst3_host_probe.cpp, receipts/receipt_20261114_reaper_trial.md
START_HEAD: 4c6b03a9a4c25ceea570eb10aa30321bf7176e73
END_HEAD: pending-commit
COMMIT: pending-commit
VALIDATION_RESULT: PARTIAL
RECOMMENDED_NEXT_ACTION: Diagnose KeyLab MIDI availability with one REAPER session, exercise physical MIDI to audible synth output, repair the host automation test with a valid smoothed-gain timing comparison, and continue independent module and ecosystem integration.

## DIRECTOR_READABLE
Cody reported that MIDI notes did not work, that UX was poor, and that work on the multiple-plugin ecosystem was shallow. A REAPER project instantiated the instrument, legacy FX and independent Vocal effect. This does not prove physical MIDI, sound, usability or connected ecosystem behavior. REAPER enumerated the Arturia KeyLab as unavailable; several REAPER instances launched during this work could contend for devices. Cody's report was captured in Lab informal evidence IE-d90372f6d057; capture is not proof of learning or adaptation.

Sound Space previously emitted free-running pitch and harmonic oscillators without input. A signal-following envelope now controls those synthesized layers; existing resonators retain their own decay. The host probe observed silence before a MIDI note at offset 64 after reset, where before the fix it observed peak 0.0483714. Sound and musical preference need listening validation.

## AI_DRIFT_LOCK
The three observed VST3 identities are not the complete intended ecosystem. Do not treat REAPER instantiation, a receipt, or the correction of one bug as completing the musician-facing suite, connection between modules, physical MIDI, or creator acceptance. Multiple previously opened REAPER sessions and retained plugin backups are secondary effects of this trial.

## TECHNICAL_RECEIPT
Windows build succeeded. run_tests.bat executed through native DSP and the host instrument probe; Sound Space regression and the previously failing sample-offset silence assertion passed. The host probe then failed its master gain automation assertion, which expects a 90 percent decrease within 64 samples despite the existing 12 ms gain smoother. This requires a causal comparison, not suppression of the assertion. Remaining tests after that point were separately executed earlier in this session against the prior source; they are not claimed as a full battery pass for this change. REAPER reported ASIO Universal Audio Volt at 48 kHz and 512 samples; MIDI device availability is session dependent. No human listening acceptance or live MIDI signal was verified.
