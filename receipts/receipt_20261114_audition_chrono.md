# Audition, MIDI ABI, ChronoState feedback, and research consumers

LAB_RECEIPT_CONTRACT_V1:
RESULT: PARTIAL
PROJECT_ID: monkeys-ear
MISSION: Make shaping sounds immediately audible, correct ChronoState feedback and recover underimplemented research paths.
FILES_CHANGED: audition.bat, tools/reaper_audition.lua, test/reaper_audition_probe.lua, test/reaper_live_audio_probe.lua, README.md, vst3/vst3_sdk_minimal.h, test/test_vst3_host_probe.cpp, src/chrono_state.cpp, include/monkeys_ear/ecosystem_handoff.h, include/monkeys_ear/ecosystem_publishers.h, include/monkeys_ear/ecosystem_state.h, include/monkeys_ear/vocal_module.h, src/vocal_module.cpp, test/test_dsp_core.cpp, receipts/receipt_20261114_audition_chrono.md
START_HEAD: 4dafab225450f65f89723a26aecc46fb14bda8ba
END_HEAD: pending-commit
COMMIT: pending-commit
VALIDATION_RESULT: PARTIAL
RECOMMENDED_NEXT_ACTION: Have Cody play the audition panel and judge ChronoState sound; trace and implement the actual cross-plugin evidence producer/transport/consumer path and the underimplemented Onion Skin EQ and pitch research, with real audio and human listening.

## DIRECTOR_READABLE
A REAPER audition panel now provides a clickable keyboard, computer keys and a repeating phrase while editing the real instrument. It routes through REAPER virtual MIDI and can also reach other armed virtual-MIDI tracks. A real REAPER monitoring probe used the same generated MIDI input for Monkey's Ear and ReaSynth: before the ABI repair the instrument was silent while ReaSynth measured .395 peak; after the repair the instrument measured .177 peak. The panel was visually inspected and its keyboard, phrase, timing, setup and note-off behavior were exercised by a REAPER-run logic probe. No physical keyboard or human listening acceptance is claimed.

Cody reported ChronoState feedback and noise before notes. After one impulse, the prior boundary force produced a persistent residual (e.g. 48 kHz, repulsion .5, late RMS .0070). The original force had a discontinuity at zero displacement. It is now continuously rebased to zero at rest while retaining full near-boundary force, and bypass clears old state. Late RMS in tested settings is below .000001 at 44.1/48/96 kHz. The exact reported pre-note scenario is not independently reproduced; other sources of noise remain possible.

## AI_DRIFT_LOCK
The research audit found the pitch-correction octave-candidate experiments are not the production tracker and Onion Skin EQ has no corresponding production module. Existing Vocal shared-pitch consumption and generic handoff exist but no VST3 producer-to-consumer connection was found. This work repairs stale evidence, invalid non-finite evidence under fast-math, and the handoff's in-progress sequencing; it does not claim cross-plugin functionality or completion of Cody's research. Lab Code Prime now carries audio evidence including independent host ABI proof into executor briefs (Lab commit 40c58a1a); later agent behavior and learning remain unobserved.

## TECHNICAL_RECEIPT
run_tests.bat: PASS including independently encoded VST3 MIDI wire payload, ChronoState silence/decay and bypass tests, Vocal fallback/expiry including bypass and oversized blocks, non-finite evidence rejection, and concurrent handoff consistency. Current Windows VST3 instrument build SHA256 A4D35DB3C1AF80EA7FCCB872C22256893250817A14D4BF12A1A3A3591BED3911 matched the installed per-user file at the REAPER test. REAPER reported audio running and effective monitoring; MIDI -> VST3 -> track meter peak .177239, control ReaSynth .394636. UI logic probe PASS. An earlier false positive from REAPER's nonzero meter floor was corrected by a 0.001 threshold and a reference instrument. Audio-device configuration, human hearing, playability, device interaction and subjective sound quality remain outside observed proof.
