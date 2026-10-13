# JamBoxFM build and validation plan

Status: implementation gates, with completed research checks identified separately. No JamBox device build or hardware flash has been performed.

Current prototype results are recorded in [BUILD_STATUS.md](BUILD_STATUS.md). This document remains the roadmap for the full firmware, including features beyond the first playable slice.

## Already done

See [RESEARCH_EVIDENCE.md](RESEARCH_EVIDENCE.md) for pinned sources, actual outcomes, warning qualifications and logs. Completed work includes REA analysis of the checked-in Sloop web distribution, a five-scenario shipped-WASM probe, targeted upstream native tests, the earlier ChoralRoot installer simulation, and comparison of the three required SDK hashes.

Those results justify proceeding to an implementation baseline. They do not certify new JamBox features or a physical update.

## Gate 1: reproducible unchanged baseline

Use the existing isolated checkout for JamBox work; no additional Git worktree is needed. Before importing source, preserve the product/research documents and inspect repository state. Vendor the selected Sloop commit with source provenance and license notices; keep upstream engine IDs and hardware layout stable.

Prepare dependencies outside the checkout:

- Python environment with pinned Pillow/fontTools versions used by asset generation.
- Official JieLi pi32v2 Linux x86-64 compiler at a recorded version/digest. Preserve HTTPS verification; use an upstream signature/checksum when available. Do not guess a checksum or run an ARM-target compiler as a substitute.
- Pinned AC79 SDK subset in its expected paths, with Apache notices and the three fail-closed SHA-256 checks in the evidence document.
- Pinned Emscripten and host Clang for the native/WASM comparison; Node for existing web tests.

At the research stage, JieLi tools, Emscripten and host Clang were absent from the inspected PATH. The implementation stage has since installed the pinned browser/native toolchain and verified the SDK; the JieLi download remains blocked. See [CLOUD_SETUP.md](CLOUD_SETUP.md) for the current environment.

Required download destinations may include `pkgman.jieliapp.com` and redirected artifact hosts, plus the chosen Emscripten distribution. Resolve the actual downloads from official sources and record their identities for repeatable setup.

From the imported, initially unchanged baseline:

```sh
export JIELI_TOOLCHAIN=/workspace/toolchains/jieli/toolchain
export AC79_SDK=/workspace/deps/ac79-sdk
./build.sh
sh tests/run_tests.sh
python3 tools/build_preview.py
CC=clang sh tests/run_preview_tests.sh
```

The unchanged browser build and preview tests have now passed; their evidence is in [BUILD_STATUS.md](BUILD_STATUS.md). The device build and full upstream runner remain future gates. The full runner depends on a generated `.fwsc`; do not label an incomplete subset as that full suite. Investigate failures without rewriting golden sound or CPU baselines merely to obtain a pass.

Retain: source SHA, compiler/SDK/asset identities, build flags, ELF/disassembly, app and loader sizes, all RAM-region sizes, `.fwsc` hash, browser JS/WASM hashes and test results. A runtime build on the same compiler/version should be reproducible; investigate differing artifacts before claiming determinism.

## Gate 2: musical core and recording contracts

Implement focused host tests against actual JamBox C modules:

| Contract | Cases and expected result |
| --- | --- |
| Role allocation | Bass1 + chord4 + lead1 with six drum voices; explicit held-group policy; bounded allocation and release fades |
| Assist mapping | Master and per-part on/off, scale/root choices, triad/seventh/ninth/power forms, drums bypassed |
| Original ownership | Change Assist/key/scale/part/octave while held, then release: all and only original pitches end |
| Input sources | USB and TRS with same channel/key remain distinct; note-on velocity zero, sustain, repeated press and disconnect handled |
| Shared pitch | Live and sequence owners release independently under the chosen retrigger policy |
| Capacity | Fifth raw chord note and full MIDI token pool follow documented refusal/replacement rules; no lost release mapping |
| No retrospective edits | Toggle Assist/root/scale/correction and compare previously stored note/micro bytes: unchanged |
| Two-chord phrase | Chord at step0 and step16 with ties, two complete loop repetitions, correct releases and no duplicate record-monitor attack |
| Record strength | Off/Gentle/Tight at positive/negative offsets and across swing/tempo/loop boundaries; adapter latency explicit |
| Overdub | Occupied-step onset retained, whole assisted chord replacement atomic, no old/new chord hybrid |
| Timing | Swing0 straight; amount changes playback only; negative step0 offset, triplets, stop/restart and ties handled |
| Undo | Complete selected-part steps/micro/locks/fill/length restored; unrelated parts unchanged |
| Parameter changes | Click-controlled sound changes/mutes/macro extremes; finite bounded output and silence after panic/tails |

Use deterministic event fixtures. When changing synthesis intentionally, compare renders and listen before approving updated golden audio. Host execution speed is useful for regressions within that host; it is not FM-1 CPU utilization.

## Gate 3: byte-exact storage and restart behavior

- Assert the encoded JAM1 length is 3,840 bytes, including checksum and all 24 new metadata bytes.
- Round-trip every Assist enum/mask, signed micro extremes, scale/root and palette ID. Reject unknown versions, invalid sizes, checksum errors and out-of-range fields.
- Confirm master bypass preserves individual preferences across save/load.
- Prove A/B saves survive interruption after every erase/program/commit step using the inherited simulated flash hooks.
- Inspect final NOINIT alignment/size: four full slots plus retained diagnostic/calibration state must fit, using at least the existing 256-byte ancillary allowance. Do not place live Assist state there.
- Boot against erased flash, valid JamBox objects and representative foreign Sloop/Felucca/ChoralRoot data. Foreign payloads are rejected read-only, not implicitly converted or immediately overwritten by autosave.
- Explicit initialization must preserve required device calibration and protected areas, and happen only after the chosen backup path is complete.
- Backup/export includes companion FM6 banks, user kits and samples referenced by projects. Test interrupted restore, missing dependencies and round-trip sound identity.
- Show pending save until flash actually commits. Confirm stopped/quiet scheduling and dirty state through a power cycle on hardware later.

Keep user data out of committed fixtures; construct synthetic fixtures or use consented, sanitized backups.

## Gate 4: browser parity and usable controls

Compile the same C Assist/session/recording paths for native tests and WASM. Drive identical event sequences and compare recorded event bytes and audio within the existing fixed-point tolerance. Run the existing native/WASM test first, then add the JamBox cases; do not rely only on the checked-in upstream WASM.

Browser interaction must verify explicit audio start, 44.1 kHz enforcement or a tested resampler, panic on focus loss, no stuck keyboard/pointer notes, consistent part selection, audio-driven transport, stable performance during knob movement, and no device-MIDI access merely to play a local demo.

Walk the actual FM-1 control mapping with its 240×240 viewport. No required action may assume touch input, knob clicks or additional track buttons. Confirm the Assist menu is discoverable, selected-part/key scope is visible, combinations consume their component actions once, and macro labels match their sound.

User audition gate: make a beat, capture two chords, add bass and improvise a lead with little explanation. Compare the proposed Dusty palette to the owner's preferences. Adjust sound choice and parameter ranges based on listening, then build Garage/Neon.

## Gate 5: physical FM-1 bring-up

Before the first flash, identify the hardware/current Sloop version through read-only device information; export and validate a full backup; establish an available recovery procedure and equipment. This requires the owner and a physical device. It does not block software-only development.

First run the unmodified pinned baseline, then small JamBox increments. Use a charged device, stable direct data connection and known firmware package. Do not perform a deliberate power-cut experiment on the owner's only instrument without separate agreement and a demonstrated recovery route.

Check:

- Calibration, all keys/buttons/encoders, MASTER, display/LED behavior and original recovery gestures.
- Headphones/speaker output, gain, clipping, DC/clicks, note response and end-to-end latency.
- A ten-minute worst-case palette workload: bass, rapid four-note chords, lead, six drums, maximum exposed FX, active screen/knob input and USB recording.
- Total interrupt-duration histogram including nested timer/USB work: provisional p99 below 70% of deadline, maximum below 85%, zero late DMA halves and zero CPU-overload shedding. Distinguish intended voice stealing from overload shedding.
- Real stack guards, actual remaining ordinary/pool RAM, code size, NOINIT fit, and guard diagnostics.
- USB/TRS MIDI and sustain, USB audio continuity, stopped/quiet flash saves, reload and dirty-state behavior.

If the workload fails, reduce unused processing/buffers, shorten preset releases or choose a cheaper engine/FX combination. A triad fallback must be explicit and musically auditioned. Do not hide the overload indicator or simply increase voice counts.

## Gate 6: installer and release

Build a static installer using the existing SysEx transport. Validate `.fwsc` identity/structure/checksums against the manifest before writing. Preserve required model and loader compatibility markers.

Test a matrix of device family, normal app, app-rescue (`FM-1_000`), actual `ota-` loader, unknown version and disconnected device. Our app rescue needs a tested family-identification path. Unknown firmware never becomes approved merely because the numeric version matches.

Use simulated peers for success, stage-one failure, re-enumeration, write interruption, retry/resume, wrong returned family/version, bad hashes, missing backup and user cancellation. On hardware verify the specifically supported Sloop→JamBox and JamBox→JamBox paths. Do not generalize ChoralRoot's V15 recommendation into a proven universal reset.

Release only after source, build manifest, checksums, licenses, package, browser assets and user instructions agree on the tested version. The GPL code and asset-specific notices must travel with the release. Preserve an explicit transition/recovery support list and unresolved limitations.

## Repeating the completed artifact research

With the pinned Sloop checkout available at the given path:

```sh
node docs/research/probes/probe-sloop-wasm.mjs /workspace/scratch/fm1-research/sloop

npm_config_cache=/workspace/.npm-cache npx --yes rea-agents@6.4.0 \
  analyze-javascript-application /workspace/scratch/fm1-research/sloop/docs/webapp \
  --analysis-timeout-ms 60000 --format json > /tmp/sloop-web-rea.json
```

The first command runs the existing shipped artifact only. The second performs static analysis and retains unknowns; neither is a substitute for the build, browser or hardware gates above. For the targeted native research checks, see the generation/compile commands and qualifications in [audio notes](research/audio-notes.md) and [Assist notes](research/assist-notes.md).
