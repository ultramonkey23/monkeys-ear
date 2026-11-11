# Receipt: Monkey's Ear - Termux/clang compile repairs at c5d67bee plus the deterministic battery green on this compute node

LAB_RECEIPT_CONTRACT_V1:
PRIME_LAW_COMPLIANT: yes
RESULT: GREEN - the 27-commit range 424e7c3b..c5d67bee is re-anchored with executed evidence on the Termux compute node: two conforming-C++ compile defects that the permissive Windows path had tolerated are repaired, and the whole deterministic battery runs green here (22 suites, exit code 0).
ACTUAL_WORK_REQUIRED: yes
ACTUAL_WORK_COMPLETED: yes
WORK_PRODUCT_TYPE: CROSS_PLATFORM_COMPILE_REPAIR_WITH_EXECUTED_BATTERY_EVIDENCE
RISK_LANE: CODE
PARENT_DIRECTOR_OBJECTIVE: Cody's standing directive "continue evolving/connecting the Lab organism"; the reconcile surface prescribed this drift's honest remedy verbatim: "Land a validation-backed receipt for 424e7c3b..c5d67bee (run the monkeys-ear test battery on a Linux compute node, record evidence under LAB_RECEIPT_CONTRACT_V1, then labctl registry apply-receipt <receipt> --write)".
CURRENT_COHERENT_MISSION: Re-anchor the monkeys-ear registry narrative with real execution. The range 424e7c3b..c5d67bee (27 commits authored on the Windows node) carried no validation evidence on a Linux compute node; building it here surfaced two compile defects (float subscript of kBeatDivisions in src/engine.cpp; void* createView return against a virtual IPlugView* in vst3/vocal_vst3_entry.cpp). Both are repaired minimally and type-only, the full battery is rebuilt at HEAD on the Termux Release toolchain and runs green, and this receipt lands as the range's validation evidence.
MISSION_RELATIONSHIP: DRIFT_REMEDIATION_WAVE
PARENT_STATUS: OPEN (the Lab-side registry pin drift REGISTRY_LATEST_KNOWN_GOOD_STALE stays owned by the tests-gated labctl reconcile-apply)
CHILD_STATUS: GREEN
SURVIVING_PARENT_GAPS: host-deadline (REAPER) proof is outside this battery by the runner's own closing boundary; the VST3 modules build but are not loaded in a live REAPER session here; no device or network proof exists for this range.
INTENDED_ADVANTAGE: the registry's next action stops serving an overtaken instruction - the next executor reads a validated head with executed evidence instead of a 27-commit blind spot, and the Termux/clang toolchain can build and verify every future wave without tripping over the same two defects.
RECOVERED_INHERITANCE: technique lineage: the deterministic suites (pattern transport, snapshot replay, stem bundle, tone algorithms, tube rack, reverb RT, vocal tracking/expression/reset, factory snapshot consistency) all rebuilt and green on Termux Release clang; the two-commit receipt convention from touch-first-3v3 wave receipts (work commit first, receipt-doc commit records it) is reused here.
CANONICAL_OWNERS: repos/monkeys-ear src/engine.cpp (integer subscript fix, std::clamp int overload at both kBeatDivisions sites); vst3/vocal_vst3_entry.cpp (IPlugView* createView return type, matching vst3_entry.cpp); receipts/receipt_20261111_termux_build_repairs_and_battery.md (this receipt); the build-termux Release tree as executed proof.
CONSUMER_TRACE: labctl project-git-commit (preview then --confirm) -> repos/monkeys-ear master work commit b4719ad -> cmake --build build-termux -j4 (all 12 targets) + ./build-termux/monkeys_ear_test_runner battery -> receipt-doc commit -> labctl project-git-push -> labctl registry apply-receipt --write.
GENERICITY_CHECK: no lab organ or tool surface added; only project work landed.
VERTICAL_SLICE_CHECK: type-only compile repairs -> clean build of all 12 targets (core, both VST3 modules, test runner, 8 probes) -> 22 deterministic suites green -> timing tables with 95.8 percent-plus margins and 0 wall misses at buffers 32-256 -> receipt committed and pushed.
PROOF_CLAIM_MATCH: proven here: monkeys_ear_test_runner exits 0 on the Termux Release toolchain (clang c++) with 22 deterministic DSP and offline CPU suites green, including the vocal tracking/expression/reset suites and the pattern transport, snapshot replay and stem bundle probes the range's commits touched; the runner's closing line declares all deterministic DSP and offline CPU suites green and bounds host-deadline proof to REAPER. The repairs change types only (integer index where a float was returned; IPlugView* where void* was declared) with identical runtime behavior asserted by the same battery. Not proven: REAPER host-deadline behavior, live VST3 loading in a host, device playback.
SURVIVING_GAP: REAPER host-deadline proof requires the host (the runner's own boundary line); no device, no network, no feel or latency judgment; the registry narrative still awaits Cody's product verdicts for the range's musician-facing work.
PROOF_BUDGET_USED: project battery only (cmake --build build-termux -j4 + monkeys_ear_test_runner); no device, no host, no network service.
WHOLE_EFFECT_PROOF: see PROOF_CLAIM_MATCH.
CODY_SMOKE_TEST_WORTHY: no
CODY_SMOKE_TEST_REQUESTED: no
WHY_MANUAL_TEST_IS_OR_IS_NOT_WORTH_IT: nothing musician-facing changed - two compile defects repaired with type-only fixes; the creator-visible judgment points for monkeys-ear remain the product roadmap, not this repair.
PROJECT_ID: monkeys-ear
MISSION: Re-anchor the monkeys-ear registry narrative 424e7c3b..c5d67bee with executed evidence: repair the two Termux/clang compile defects at HEAD (kBeatDivisions float subscript in src/engine.cpp; vocal VST3 createView return type in vst3/vocal_vst3_entry.cpp) and run the deterministic battery green on this compute node.
EXECUTION_MODEL: llxprt-code/xiaomi-mimo-v2.6-pro (Termux android host)
FILES_CHANGED: receipts/receipt_20261111_termux_build_repairs_and_battery.md
PHASE: FINAL
START_SYNC_STATUS: SYNCED
FINAL_SYNC_STATE: SYNCED
PRIOR_LOCAL_ONLY_MIRRORED: n/a
BASE_SHA: c5d67bee2c8a022556f5df836bf7e74d26db723f
PULL_FIRST_RESULT: N/A (monkeys-ear working tree clean at battery start except the two repair files under review)
START_HEAD: c5d67bee2c8a022556f5df836bf7e74d26db723f
END_HEAD: b4719adc19f6a88110c081d1f3610b5e85559ffe
COMMIT: b4719adc19f6a88110c081d1f3610b5e85559ffe
COMMIT_CREATED: yes
PUSH_STATUS: PENDING_THIS_RECEIPT_COMMIT
VALIDATION_RESULT: GREEN - monkeys_ear_test_runner exit code 0 on Termux Release clang; 22 deterministic DSP and offline CPU suites green; timing tables show 95.8 percent-plus margins and 0 wall misses at buffers 32-256 samples; the runner's own closing boundary scopes host-deadline proof to REAPER.
VALIDATION_COMMAND: cmake --build build-termux -j4 && ./build-termux/monkeys_ear_test_runner
VALIDATION_RESULT_LINE: {"exit": 0, "suites_green": 22, "wall_misses": 0, "closing": ">>> ALL DETERMINISTIC DSP & OFFLINE CPU TESTS PASSED; HOST DEADLINE PROOF REQUIRES REAPER. <<<"}
FINAL_STATE: PENDING_THIS_RECEIPT_COMMIT
ROLLBACK_COMMAND: git revert b4719adc19f6a88110c081d1f3610b5e85559ffe
DIRECTOR_READABLE:
- Monkey's Ear did not compile at HEAD on this compute node. Two type defects the permissive Windows path tolerated were standing between every Linux/Android validation and the 27 commits of real audio work behind them: kBeatDivisions was subscripted with a float (the local clamp returns float), and the vocal VST3 entry declared createView returning void* against a virtual that returns IPlugView*. Both repaired type-only; runtime behavior unchanged.
- With the repairs, the entire deterministic battery rebuilds and runs green on the Termux Release toolchain: 22 suites covering oscillator math, chrono-stateful operators, vocal tracking and expression, MIDI semantics, beat clock and swing, factory snapshot consistency, preset round-trip and bounded microfields - plus the pattern transport, snapshot replay and stem bundle probes the range's newest commits introduced.
- This receipt is the validation-backed evidence the registry asked for: the range is no longer a blind spot, and the Termux node can verify future waves.
- What I need from you stays product: the range's musician-facing work (mobile DAW canon, stem export, production timing) still awaits your judgment; this repair only unblocks verification.
