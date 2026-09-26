# Voice/Vocal changelog

Voice/Vocal is versioned independently from the unfinished Monkey's Ear suite.
Versions here identify exact evaluation and release candidates; they do not
expand the proof boundary recorded in the validation bundle.

## 0.1.0-preview.1

First explicitly versioned Windows x64 VST3 validation candidate.

- Provides the standalone 11-control Voice/Vocal effect.
- Reports zero plug-in latency for its current LIVE processing path.
- Preserves all 11 parameters through the VST3 component/controller state
  contract.
- Rejects truncated or non-finite serialized state without partial mutation.
- Includes a checksum-bound tester bundle with install, removal, evidence, and
  failure-report instructions.

Still unproved: REAPER scan/load/save/reopen/render, clean-user installation,
real-session CPU behavior, audible quality, musical usefulness, and operation
in other production hosts.
