# Monkey's Ear DSP, Waveform Generator & Morphing, Onion Skin EQ, Pitch & Native UI Upgrade

LAB_RECEIPT_CONTRACT_V1:
RESULT: PASS
PROJECT_ID: monkeys-ear
CLOSES_MISSION: CPM-monkeys-ear-c5562598b59c
MISSION: Deepen oscillators with band-limited waveform generator & real-time morphing, fix Chrono/SoundSpace/TubeRack DSP defects and preset balance, integrate 6-band ERB Onion Skin EQ + octave persistence gate + cross-plugin EcosystemBusClient, upgrade native editor UI/UX, add native UI for monkeys_ear_vocal, verify all tests, install VST3s, and launch REAPER audition.
FILES_CHANGED: CMakeLists.txt, run_tests.bat, tools/reaper_audition.lua, include/monkeys_ear/wavetable.h, src/wavetable.cpp, include/monkeys_ear/onion_skin_eq.h, src/onion_skin_eq.cpp, include/monkeys_ear/ecosystem_bus.h, src/ecosystem_bus.cpp, include/monkeys_ear/chrono_state.h, src/chrono_state.cpp, src/sound_space.cpp, src/tube_rack.cpp, include/monkeys_ear/tonal.h, src/tonal.cpp, include/monkeys_ear/voice.h, src/voice.cpp, include/monkeys_ear/preset.h, src/preset.cpp, include/monkeys_ear/engine.h, src/engine.cpp, include/monkeys_ear/vocal_module.h, src/vocal_module.cpp, vst3/preset_param_snapshot.h, vst3/vst3_entry.cpp, vst3/native_editor.cpp, vst3/vocal_vst3_entry.cpp, test/test_dsp_core.cpp, test/test_snapshot_replay.cpp, test/test_vst3_host_probe.cpp, test/test_vocal_tracker_probe.cpp, test/test_upgrade_probe.cpp
START_HEAD: 686dba2
END_HEAD: pending-commit
COMMIT: pending-commit
VALIDATION_RESULT: PASS
RECOMMENDED_NEXT_ACTION: Live subjective listening, playtesting, and tweaking in REAPER across the new WAVE/GEN, ONION/ECO, and Vocal Companion views.

## DIRECTOR_READABLE
Implemented a comprehensive DSP, pitch-tracking, ecosystem, and UX/UI overhaul across both `monkeys_ear.vst3` (`202` parameters, `kStateVersion = 8`) and `monkeys_ear_vocal.vst3` (`11` parameters + new Win32 `VocalEditor` UI). Replaced shallow fixed oscillator shapes with a 7-mip band-limited `WaveformBank` supporting 9 wave families (`Saw`, `Pulse`, `Triangle`, `Sine`, `Formant`, `BrassFold`, `Organ`, `GlassBell`, `CustomGen`), real-time `A -> B` morphing, asymmetric phase warp, wavefold, up to 5-voice unison detune, sub-harmonics, env/LFO morph modulation, and sub-sample hard-sync PolyBLEP compensation. Fixed `ChronoStateBody` high-cutoff integration, `SoundSpaceProcessor` sample-rate invariance and idle gating, `TubeRack` smooth saturation, and `TUBE FOUNDRY` / `GLASS CHAMBER` gain balance. Integrated zero-latency 6-band ERB `OnionSkinEQ` with low/transient protection and cosine >= 0.86 Projection Gate, upgraded `ExternalSubharmonic` pitch tracker (`strong2=0, missingF0=1`), wired lock-free `EcosystemBusClient` shared memory between Synth and Vocal instances, and added `WAVE / GEN` and `ONION / ECO` interactive pages plus Shift-fine-drag, hover wheel, and right-click reset.

## TECHNICAL_RECEIPT
All 12 test executables executed cleanly via `run_tests.bat` (exit code 0) after final edits: `monkeys_ear_test_runner`, `monkeys_ear_tube_rack_probe`, `monkeys_ear_tone_algorithms_probe`, `monkeys_ear_reverb_rt_probe`, `monkeys_ear_snapshot_replay_probe`, `monkeys_ear_pattern_transport_probe`, `monkeys_ear_stem_bundle_probe`, `monkeys_ear_host_probe`, `monkeys_ear_vocal_host_probe`, `monkeys_ear_vocal_target_chain_probe`, `monkeys_ear_vocal_tracker_probe`, and `monkeys_ear_upgrade_probe`. Installed freshly built `monkeys_ear.vst3` (677,894 bytes) and `monkeys_ear_vocal.vst3` (208,835 bytes) into both `C:\Users\harin\AppData\Local\Programs\Common\VST3\` and `C:\Program Files\Common Files\VST3\`.
