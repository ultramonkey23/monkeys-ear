# Monkey's Ear Wave 2 Deep Oscillators, 24-Band ERB Onion EQ & GDI+ ComfyUI Studio UI

LAB_RECEIPT_CONTRACT_V1:
RESULT: PASS
PROJECT_ID: monkeys-ear
CLOSES_MISSION: CPM-monkeys-ear-c5562598b59c
MISSION: Deepen Monkey's Ear oscillators with 2x oversampled wavefold/warp, H1..H8 custom harmonic generator, independent Osc 2 target/detune/ring-mod, morph curve, true stereo unison, upgrade Onion Skin EQ from 6 to 24 Glasberg-Moore ERB critical bands with Tilt/Q/Air & MEC3 shared bus, build bespoke GDI+ studio UI backed by local ComfyUI art, verify all tests, install VST3s, and launch REAPER visibly on Cody's desktop.
FILES_CHANGED: .gitignore, CMakeLists.txt, art/source/ui/monkeys_ear_ui.json, art/ui/monkeys_ear_crest.png, art/ui/monkeys_ear_crest.png.provenance.json, art/ui/studio_chassis_plate.png, art/ui/studio_chassis_plate.png.provenance.json, include/monkeys_ear/ecosystem_bus.h, include/monkeys_ear/onion_skin_eq.h, include/monkeys_ear/preset.h, include/monkeys_ear/voice.h, include/monkeys_ear/wavetable.h, src/engine.cpp, src/onion_skin_eq.cpp, src/preset.cpp, src/voice.cpp, src/wavetable.cpp, test/test_dsp_core.cpp, test/test_snapshot_replay.cpp, test/test_upgrade_probe.cpp, test/test_vst3_host_probe.cpp, vst3/native_editor.cpp, vst3/preset_param_snapshot.h, vst3/ui_assets.h, vst3/vst3_entry.cpp
START_HEAD: c18321b171d024ff8d66f21cae76b7412cd2557d
END_HEAD: 35fa863b1dc1afe7e2838291e78ec82683f05400
COMMIT: 35fa863b1dc1afe7e2838291e78ec82683f05400
VALIDATION_RESULT: PASS
RECOMMENDED_NEXT_ACTION: Playtest the visible REAPER session across PLAY, WAVE / GEN, and ONION / ECO tabs.

## DIRECTOR_READABLE
Upgraded Monkey's Ear (`monkeys_ear.vst3` -> `245` parameters, `kStateVersion = 9`, preserving `v8` migration compatibility) across three major pillars:
1. **Deep Oscillators & Custom Harmonic Generator (`202..217`):** 2x oversampled `PolyBLEPOscillator` wavefolding/phase-warp, 8 interactive partial drawbars (`H1..H8`), `morph_curve` (`Linear`/`S-Curve`/`EqualPwr`/`Step-8`), independent `Osc 2` target & fine detune, `Ring Mod Mix`, `Stereo Unison Width` + stereo split voice path, `Analog Drift`, `Auto-Morph Rate`, and `Osc 1<->2 Balance`.
2. **24-Band Cochlear ERB Onion-Skin Dynamic + Surgical EQ (`218..244`):** Upgraded from 6 bands to 24 Glasberg-Moore ERB critical bands (`40 Hz .. 9.6 kHz`), adding `Spectrum Tilt`, `Carve Q Focus`, `Air Shimmer Shelf`, interactive 24-band surgical sculpt (`-12..+12 dB`), cosine >= 0.86 Projection Gate, and `MEC3` v2 shared memory bus (`24` ERB bands).
3. **GDI+ Studio Hardware UI with Local ComfyUI Art:** Generated and promoted `studio_chassis_plate.png` and `monkeys_ear_crest.png` via `anim_forge ui` / `comfy_bridge.py` (with paired `ultramonkeydog-lab` commit `66797e78`), embedded them into `vst3/ui_assets.h`, and built anti-aliased GDI+ views including interactive 3D isometric waterfall wavetable display, XY warp/fold scope, morph ribbon, sweep-paint `H1..H8` partial bars, and 24-band ERB curve editor. Verified visually via `PrintWindow` PNG captures (`ui_play.png`, `ui_wave_gen.png`, `ui_onion_eco.png`).

## TECHNICAL_RECEIPT
- Full build & suite via `run_tests.bat`: exit code 0 across all 12 executables (`monkeys_ear_test_runner`, `monkeys_ear_tube_rack_probe`, `monkeys_ear_tone_algorithms_probe`, `monkeys_ear_reverb_rt_probe`, `monkeys_ear_snapshot_replay_probe`, `monkeys_ear_pattern_transport_probe`, `monkeys_ear_stem_bundle_probe`, `monkeys_ear_host_probe`, `monkeys_ear_vocal_host_probe`, `monkeys_ear_vocal_target_chain_probe`, `monkeys_ear_vocal_tracker_probe`, and `monkeys_ear_upgrade_probe` 7/7 checks passing).
- Installed rebuilt `monkeys_ear.vst3` (`810,721` bytes) and `monkeys_ear_vocal.vst3` (`210,372` bytes) into both `C:\Users\harin\AppData\Local\Programs\Common\VST3\` and `C:\Program Files\Common Files\VST3\`.
- Lab-side `ui_art` wiring commit in `ultramonkeydog-lab`: `66797e78060840429d2f0fb51c6d1411005c5278` (`tools/asset_forge.py`, `tools/comfy_bridge.py`).
