LAB_RECEIPT_CONTRACT_V1:
RESULT: PASS
PROJECT_ID: monkeys-ear
MISSION: Keep internal performance inputs out of the native editor's parameter search
FILES_CHANGED: vst3/native_editor.cpp, test/test_vst3_host_probe.cpp, receipts/receipt_20261124_hidden_search_ux.md
START_HEAD: fd277917de69ae9c1f4cd48e0094643bd832508e
END_HEAD: pending
COMMIT: pending
VALIDATION_RESULT: PASS
RECOMMENDED_NEXT_ACTION: Exercise broader musician workflows in the real REAPER host and assess usability with a musician.

The EXPLORE page formerly suppressed hidden VST3 parameters only while the search was empty. Searching by their names exposed pitch bend and channel pressure as ordinary editable controls, despite their role as hidden hardware performance inputs. The editor now excludes hidden parameters for both browsing and search; host automation and MIDI routing remain unchanged.

An independent Codex test author added a real HWND/VST3 host-probe regression. It searches for both hidden performance parameters and verifies clicking does not emit an edit; a visible pitch-bend range search is a positive control and still edits parameter 73. The current-source `run_tests.bat` build and complete required DSP and host suite passed on Windows. This is native host-probe evidence, not a REAPER session, hardware MIDI performance, listening, or subjective usability acceptance.
