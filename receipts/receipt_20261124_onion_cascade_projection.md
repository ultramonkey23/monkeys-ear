LAB_RECEIPT_CONTRACT_V1:
RESULT: PARTIAL
PROJECT_ID: monkeys-ear
MISSION: Recover Onion Skin EQ research projection fidelity and prevent unintended cumulative dynamic attenuation
START_HEAD: b6c2cd5bcb9ef5c1343a3521f13b5ef75ae4ffd9
END_HEAD: e1fc57a8856f37fc4a77b41079fd53b67ff176d7
COMMIT: e1fc57a8856f37fc4a77b41079fd53b67ff176d7
VALIDATION_RESULT: PARTIAL
FILES_CHANGED: include/monkeys_ear/onion_projection.h, include/monkeys_ear/onion_skin_eq.h, src/onion_skin_eq.cpp, test/test_onion_projection.cpp, test/test_dsp_core.cpp, CMakeLists.txt, run_tests.bat, research/onion-skin-eq/RESEARCH_HISTORY.md, research/STATUS.json, receipts/receipt_20261124_onion_cascade_projection.md
RECOMMENDED_NEXT_ACTION: Run full current-source Windows run_tests.bat, including the new mandatory projection probe; compare real stems and REAPER behavior, and trace the existing MEC4 evidence freshness/provenance and pitch-reference authority before further expansion.

## DIRECTOR_READABLE

The former projection gate could accept a non-improving proposal and could
apply an unevaluated scale. Commit 4e0c161 separates accepted decisions from
candidates. A real native audio render then exposed an inherited problem:
adjacent peaking filters multiply attenuation, which the old independent-band
projection ignored. Commit e1fc57a projects the shared production RBJ cascade
at analyzer centers, budgets the aggregate dynamic response and directs
harmonic protection toward nearby filters rather than simply lowering all
carve strength. The new mandatory native probe exercises both analytic and
actual DSP behavior. Manual EQ remains independent; no host parameter IDs,
state schemas or module identities changed.

## TECHNICAL_RECEIPT

External Chat Partner, explicitly authorized by Cody to work on master.
Executed GCC C++20 -O3 -ffast-math in an isolated Linux container using
source files reconstructed from GitHub reads and matched by Git blob hash.
The 4,000-case occupancy selector and 2,700-case independent complex
transfer-function projection tests passed; the latter accepted 2700/2700.
A production 878 Hz fixture changed from approximately -21.1 dB baseline
attenuation to -6.0 dB; a separate manual +3 dB target remained available.
Actual two-tone 255/4275 Hz results at Q 1.85: without harmonic guard
-5.008/-5.969 dB; with guard -0.905/-5.204 dB. Both implementation commits
were advanced by non-forced expected-parent updates to master. No branches,
merges, issues, PRs, or unrelated source changes created.

## AI_DRIFT_LOCK

Scope of proof is native isolated C++ DSP and synthetic/analytic evidence.
Full current-source Windows VST3 probes, physical MIDI or REAPER, human
listening, deadline proof and the full V9 auditory/harmonic research are
UNVERIFIED. The dynamic-only center-frequency bound does not prove the
entire time-varying or manual-plus-dynamic transfer response. An isolated
broadband benchmark indicates extra compute; full host CPU must be checked.
The strongest-proposal aggregate budget is a design choice to evaluate,
not a permanent artistic law. No Lab-native learning ingestion or second-use
behavior was executed by this external chat session.
