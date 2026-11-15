# Standalone Monkey's Ear Onion Skin EQ VST3 FX (24-Band ERB + Sidechain + MEC3)

LAB_RECEIPT_CONTRACT_V1:
RESULT: PASS
PROJECT_ID: monkeys-ear
CLOSES_MISSION: CPM-monkeys-ear-c5562598b59c
MISSION: Build standalone Monkey's Ear Onion Skin EQ VST3 effect plugin (`monkeys_ear_onion_eq.vst3`) usable on any track in REAPER with Main Stereo + optional Sidechain 3/4 + MEC3 Ecosystem peer linking + Self De-Harsh + GDI+ hardware UI, verify all tests, install all VST3s, and launch REAPER visibly.
FILES_CHANGED: CMakeLists.txt, build.bat, run_tests.bat, tools/reaper_audition.lua, include/monkeys_ear/onion_eq_module.h, include/monkeys_ear/onion_eq_product_version.h, src/onion_eq_module.cpp, test/test_onion_eq_vst3_probe.cpp, vst3/onion_eq_vst3_entry.cpp
START_HEAD: d67f8e2209fc56e3e5cf212308fea5e7d2f8a82a
END_HEAD: b9b4b77818974da38a74cdeeed9f0ef652f135f5
COMMIT: b9b4b77818974da38a74cdeeed9f0ef652f135f5
VALIDATION_RESULT: PASS
RECOMMENDED_NEXT_ACTION: Insert or click OPEN ONION EQ in REAPER to sculpt or sidechain-carve any audio/instrument track.

## DIRECTOR_READABLE
Created `monkeys_ear_onion_eq.vst3` (`"Monkey's Ear Onion Skin EQ"`, `Fx|EQ`, `41` automatable parameters, `MEOQ` versioned state) so Onion Skin 24-Band ERB Dynamic + Surgical EQ can run standalone on any track in REAPER:
- **Core FX Module (`OnionEqModule`):** True dual-channel stereo `OnionSkinEQ`, selectable `Stereo L/R` / `Mid Only` / `Side Only` processing, `Dry/Wet Mix`, and `Output Trim` (`-12..+12 dB`).
- **Four Dynamic Reference Sources:** `Auto (SC+Eco)`, explicit auxiliary `Sidechain In` (`3/4`), `Ecosystem Peer` (`MEC3` shared memory Ch 1..8), and `Self De-Harsh` (single-track dynamic narrowband ERB resonance suppression without needing an external sidechain).
- **Per-Instance Telemetry & GDI+ Studio UI (`980x620`):** Lock-free `IConnectionPoint` per-instance telemetry (`self_occ`, `ref_occ`, `carve_db`, `total_eq_db`, `identity_cosine`, `conflict_reduction`, `transient_hold`, `IN/OUT` peaks) paired with the ComfyUI chassis plate/crest, interactive sweep-paint 24-ERB curve (`FLAT`, `WARM`, `CLARITY`, `POCKET`), and `OPEN ONION EQ` integration in [reaper_audition.lua](tools/reaper_audition.lua).

## TECHNICAL_RECEIPT
- Build & full regression via `run_tests.bat`: exit code `0` across all 13 test executables, including `monkeys_ear_onion_eq_host_probe.exe` (validating `kAux` Sidechain 3/4 bus topology, `MEOQ` state serialization/rejection, 24-band surgical boost, bypass, Sidechain 3/4 carve `0.176602 -> 0.0121322`, Self De-Harsh `0.176865 -> 0.0808799`, `MEC3` peer carve, sibling independence, and `980x620` GDI+ HWND attachment).
- Captured visual UI verification via `PrintWindow` at `scratch/onion_eq_standalone.png`.
- Installed rebuilt `monkeys_ear_onion_eq.vst3` (`351,996` bytes) alongside `monkeys_ear.vst3` (`810,721` bytes) and `monkeys_ear_vocal.vst3` (`210,372` bytes) into both `C:\Users\harin\AppData\Local\Programs\Common\VST3\` and `C:\Program Files\Common Files\VST3\`.
