# Monkey's Ear — musician preview, evidence and distribution kit

Status: **product preview preparation / November 1, 2026**. This is not a public beta offer, a product release, a claim of creator-accepted audio quality, or permission to solicit testers. The product repo owns DSP and host truth. Cody owns subjective play/listening acceptance and release decisions.

## The instrument in musician terms

Monkey's Ear is a connected ecosystem of independently useful VST3 instruments and effects: playable, controllable, recallable sound first; deeper and stranger mechanisms available when you want them. This is neither just a low-latency demonstration nor a promise that every imagined module is finished.

**Audience A — metal / experimental / cinematic music:** Perform unusually heavy or unstable synth parts, keep low-end fundamentals separate from changing harmonic texture, drive or soften the tone, and shape motion in time with an existing session. Prove the usefulness with actual mixed audio, not abstract features.

**Audience B — electronic sound designers and producers:** Use the immediate PLAY macros for tone, movement, grit, echo and space, then enter PERFORM, KEYS or DEEP to get more precise. Prove preset recall, host automation and reproducible results.

**Audience C — musicians who want usable VST3 workflows:** Open the plugin from REAPER, play ordinary MIDI, manipulate the controls, save the project, reopen and get the same sound. Do not call this solved until direct host execution and human judgement match the promise.

Source facts from the October 30 integrated branch: 8 presets, 8 PLAY macros, a native Windows VST3 editor, 178 VST3 parameters, MIDI event input, tempo-follow capabilities, sample-offset parameter events, four tube positions, and DRIFT/FRACTURE performance gestures. Current source does **not** prove a shipped installer, subjective quality, stable REAPER recall or supported DAW matrix.

## Capture 1 — the first 20 seconds (future public instrument clip)

**Goal:** A listener understands that this is an instrument that can be played, not a menu or lab instrument.

- Start with a real REAPER track and an actual human MIDI take on CLOCKWORK SUB, with both audio and plug-in interface visible. Use original, rights-owned music or a newly played phrase.
- Play the same phrase with DRIFT / FRACTURE neutral, then with one deliberate gesture. Use the same preset, note data, signal chain and output level. Label the changed control, not the sound as "better."
- The camera follows **sound before DSP explanation**: waveform/keyboard/tone response; native PLAY macro; gesture; hear the result. Avoid a synthetic example or a stock-song backing track.
- Preserve raw before/after WAVs and the REAPER project as the source of truth. Check gain matching; no volume-louder-is-better tricks.

**Go/no-go:** VST3 builds on target Windows; REAPER scans it; no hangs; note and preset behave as shown; audio is real and intelligible; creator accepts what it sounds like. If not, keep the clip private as development evidence.

## Capture 2 — distinct sounds, not eight labels (future 30–45s product demo)

- TUBE FOUNDRY: one performed phrase, then a purposeful Heat/character move. Note the tube path shares a saturator with other effects and current alias behavior needs further testing.
- GLASS CHAMBER: a contrasting sustained phrase with long release and Sound Space; let the tail be heard instead of hard-cutting it.
- CLOCKWORK SUB: switch between neutral and gesture on a matched note phrase. Show DAW tempo-follow using 80, 120 and 160 BPM only after real host checks.
- End with a saved project reopen and the same useful patch, after it is genuinely verified.

**Go/no-go:** Level-matched playback, no unintended clicks, tails intact, settings recalled, no false "no latency in all conditions" or subjective "professional sound" claim. Capture editor screenshots from the *real plugin*, not a painted mock-up.

## Capture 3 — controlled listening panel

Use the existing `research/listening/blind_kit.py` to generate level-matched, anonymous A/B comparisons from real render WAVs. The tool does not replace listening or declare which tone wins.

Prepare and rate at least:
1. CLOCKWORK SUB neutral vs CLOCKWORK SUB with DRIFT/FRACTURE (same phrase and output target);
2. TUBE FOUNDRY vs an appropriate clean control (same phrase and matched perceived loudness);
3. GLASS CHAMBER in an actual mix and solo (same control chain);
4. save/reopen state after host restart vs original render.

Record for each: headphones/speakers, sample rate, buffer, REAPER version, playback context, loudness level, perceived difference, preference and reason, glitches/clicks, reproducibility, and whether the change is useful in a song.

Acceptance requires Cody's musical judgement and real REAPER sessions. Automated spectral figures and historical latency tables are not sufficient.

## Real validation sequence before inviting outsiders

1. Run `build.bat` and `run_tests.bat` on current canonical `master`, noting exact source commit, toolchain, build artifact and result.
2. Execute the Windows VST3 host regression; check native editor open/close/resize and complete initial factory snapshots, standard automation, tempo updates and saved project recalls.
3. Run actual REAPER with MIDI instrument and standalone Vocal module separately. The Vocal module's claim ceiling is distinct from the full instrument.
4. Test at 32, 64 and 128 samples in a real audio session, including performance while changing controls. Record interruptions and audio-thread stress instead of transferring older benchmark claims.
5. Capture finished stereo WAVs and run blind listening across at least two meaningful musical contexts. Document failures, not just favorites.
6. Verify a clean install and uninstall, versioned bundle, supported Windows host instructions, licensing, support contact and bug-reproduction template.
7. Only then prepare a musician-accessible download, terms, beta invitation and an actual availability claim approved by Cody.

## First public distribution paths (not submissions yet)

| Channel | Product fit | Entry conditions | What to publish when ready |
| --- | --- | --- | --- |
| [KVR Audio New Developer Product Release Forum](https://www.kvraudio.com/blog/kvr-audio-ai-policies-upcoming-changes-to-our-news-coverage-and-guidelines-for-developers-69478/) | Targeted VST3 users and developers | Register as a human; follow the **October 28, 2026** rule for new/unapproved developers. KVR explicitly instructs AI agents **not** to email them. | Working Windows VST3, exact host/OS, current screenshots, real audio demo and support contact; creator-authored or creator-approved post |
| [KVR Developer/product database](https://www.kvraudio.com/submissions) | Durable search and product following | Actual release and account approval process | Factual standalone instrument/effect listing and supported formats |
| [Bedroom Producers Blog](https://bedroomproducersblog.com/about/) | Free/affordable plugin enthusiasts and independent music producers | Check current contact route; must have tested downloadable product and distinct sound demonstration | Short honest product facts, WAV/video examples and reliable download/press assets |
| [Product Hunt](https://help.producthunt.com/en/articles/17350772-launch-guide) | General makers interested in unusual usable music software | Users must be able to try the product; credible demo; no upvote solicitation | Instrument use case rather than complex DSP claims |
| DAW/sound-design communities | Actual player feedback | Read each community's current rules before posting; human participation required | Ask about one real patch/workflow with reproducible audio comparison, not a disguised advertisement |

**Do not:** send KVR an AI-agent email; automate unsolicited community posts; promise availability, compatibility, commercial terms, pricing or awards; use unrelated old game media; imply research samples are listening-accepted product audio.

## Short factual media description — approval copy, not a release announcement

**Monkey's Ear** is an independent VST3 instrument and audio-effects ecosystem from Ultramonkeydog Studios. It combines playable synthesis, subharmonic weight, sound-shaping effects and performance controls with deeper modulation and experimental sound movement. The current Windows instrument is in active development and real-host/listening validation; it is not yet a public commercial release.

**Known limitations to disclose:** Windows-specific native editor; current REAPER experience unverified for the latest integrated build; no accepted cross-host compatibility claim; no verified commercial download; Voice/Vocal is a separate pre-release module.

## What to measure after a *real* beta

Track each participant's working host/OS/buffer, successful install and first sound, time to first useful patch, save/reopen fidelity, reproducible faults, sound preference in a real musical mix, support burden and explicit willingness to keep using the product. A "download" without successful creation or repeat use is not proof of musical usefulness.

The creator owns the artistic and release verdict. This document helps turn that verdict into honest public evidence; it does not substitute for one.
