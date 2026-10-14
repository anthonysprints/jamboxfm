# JamBoxFM build and validation plan

Status: implementation gates, with completed checks identified separately. JamBox target builds, milestone 2's update/persistence isolation, and milestone 3's identity, local candidate packaging, simulated transitions and read-only inventory are implemented. Software results are indexed in [update-isolation evidence](build-evidence/update-isolation/README.md) and [candidate evidence](build-evidence/candidate/README.md). Since those milestones, the owner has reported a successful Chrome installation and a normal JamBox update with working BPM and SNAP lights. Complete hardware and recovery validation remains outstanding.

Current prototype results are recorded in [BUILD_STATUS.md](BUILD_STATUS.md). This document remains the roadmap for the full firmware, including features beyond the first playable slice.

The [two-Home-view implementation](HOME_VIEWS_PLAN.md) passed its native/browser
checks and updater-enabled target build. Results belong in the
[Home views build record](build-evidence/home-views.md); publication is verified and physical
audition remains pending. Its specific acceptance requirements are listed under
Gate 4 below, separately from earlier UI builds.

The October 14 [chord-builder increment](CHORD_BUILDER.md) needs focused checks
for every black-key root in OFF/SNAP on Chords and Lead, exact scale-derived
and fixed-shape pitches, WHITE modifier compatibility, duplicate snapped roots
and overlapping chord tones in either release order, held setting changes,
strum, arp, recording, panic and sounding-note lights. Preserve old CHORD IDs
and the existing single-note path. Check full POLY chords when enabled on
Bass/Lead through the normal/held SEL, preview/editor and sound-loading paths;
CHORD OFF must leave the selected voice mode unchanged. Check ROOT/AUTO/manual
inversion pitches, old `P_VLEAD` values, SEL 2 controls and the triad +3 octave
shift. The original chord build's outcomes are in its
[build record](build-evidence/chord-builder.md).

The later October 14 lighting follow-up needs separate checks for every
sounding chord tone plus physical pressed keys, simultaneous sequencer/MIDI
notes on the selected part, brief note flashes, WHITE mapping, transpose,
inversions and out-of-range notes without wrapping. Confirm single-note SNAP
still lights only its pressed and corrected pitches, without unrelated input
aliases, and that drum/step/layer indications retain their existing behavior.
Native/browser outcomes and target memory fit for this follow-up must be
recorded before publication; see the
[lighting build record](build-evidence/sounding-note-lights.md) for results.
The owner's successful earlier installation and tempo/light update do not
establish physical validation of the new chord behavior.

The narrower [first-device build plan](FM1_DEVICE_PLAN.md) records the sequence: target builds and update/persistence isolation, candidate identity/installer simulations and inventory, an experimental hardware trial, then versioned persistence. The first trial and an update now have owner-reported success; the remaining physical checks and versioned persistence are still gates. The temporary-session milestone does not require every future musical feature below.

## Already done

See [RESEARCH_EVIDENCE.md](RESEARCH_EVIDENCE.md) for pinned sources, actual outcomes, warning qualifications and logs. Completed work includes REA analysis of the checked-in Sloop web distribution, a five-scenario shipped-WASM probe, targeted upstream native tests, the earlier ChoralRoot installer simulation, and comparison of the three required SDK hashes.

Those initial research results justified the implementation baseline. They do not certify later JamBox features or a physical update.

The target-build follow-up compiled the unchanged Sloop baseline and JamBox diagnostic target. Milestone 2 separates `FELUCCA_PERSIST=0` from enabled low-level flash/OTA, rejects app-data saves/restores, clears retained musical state and accepts only narrowly validated read-only panel calibration. Its simulated NOR, UI/editor, OTA, loader and USB/parser tests validate software contracts; they cannot certify recovery on the owner's device.

Milestone 3 adds exact JamBox family/mode/capability responses, host framing checks, local package/build-source consistency gates and transition simulations. The public-site package gains a read-only device page with explicit port selection and a private report export. [FM1_CANDIDATE.md](FM1_CANDIDATE.md) documents the implemented boundaries; test counts, artifact hashes and integration outcomes belong in the candidate evidence record.

## Gate 1: reproducible unchanged baseline

Use the existing isolated checkout for JamBox work; no additional Git worktree is needed. Before importing source, preserve the product/research documents and inspect repository state. Vendor the selected Sloop commit with source provenance and license notices; keep upstream engine IDs and hardware layout stable.

Prepare dependencies outside the checkout:

- Python environment with pinned Pillow/fontTools versions used by asset generation.
- Official JieLi pi32v2 Linux x86-64 compiler at a recorded version/digest. Preserve HTTPS verification; use an upstream signature/checksum when available. Do not guess a checksum or run an ARM-target compiler as a substitute.
- Pinned AC79 SDK subset in its expected paths, with Apache notices and the three fail-closed SHA-256 checks in the evidence document.
- Pinned Emscripten and host Clang for the native/WASM comparison; Node for existing web tests.

The pinned browser/native tools, SDK and owner-supplied JieLi 20260730.1 compiler are installed. See [CLOUD_SETUP.md](CLOUD_SETUP.md) for setup, archive provenance and the download-network limitation on a fresh machine.

Required download destinations may include `pkgman.jieliapp.com` and redirected artifact hosts, plus the chosen Emscripten distribution. Resolve the actual downloads from official sources and record their identities for repeatable setup.

From the imported, initially unchanged baseline:

```sh
export JIELI_TOOLCHAIN=/workspace/toolchains/jieli/toolchain
export AC79_SDK=/workspace/deps/ac79-sdk
bash build.sh
sh tests/run_tests.sh
python3 tools/build_preview.py
CC=clang sh tests/run_preview_tests.sh
```

The unchanged browser build and preview tests passed; their evidence is in [BUILD_STATUS.md](BUILD_STATUS.md). The first baseline/JamBox target builds and the full inherited runner's actual outcome are recorded in [device evidence](build-evidence/device-build/README.md). The full runner depends on a generated `.fwsc`; do not label an incomplete subset as that full suite. Investigate failures without rewriting golden sound or CPU baselines merely to obtain a pass.

That full baseline run remains a failure: the IRQ static-cost expectation is stale relative to the byte-identical published Sloop IRQ, and the separate PWA environment workaround still leaves Chromium's `in-incognito` assertion failing. Neither baseline nor assertion was relaxed. Passing new isolation tests does not erase those results.

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

## Gate 3: temporary-session isolation, then versioned persistence

The milestone 2 policy is implemented separately from future JAM1 saving. Reproduce its software checks with:

```sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/cloud/report_device.py > build/device-memory.json
sh tests/jam_storage_run.sh build/felucca.fwsc
sh tests/run_jam_emu_tests.sh
```

The wrapper fixes `JAMBOX_PROTO=1`, `FELUCCA_FLASH=1`, `FELUCCA_OTA=1`, `FELUCCA_PERSIST=0`, and both OTA dry-run/RAM-only flags to `0`. It retains the diagnostic target without `--with-updater`. Both outputs share filenames and remain unsuitable for owner installation. Final sizes and test outcomes belong in [the isolation evidence](build-evidence/update-isolation/README.md), not estimates copied from an earlier build.

Required isolation contracts:

- Seed valid foreign projects, settings, user banks and sample metadata plus retained RAM; normal JamBox boot starts Dusty, clears retained projects/settings and does not import foreign musical state. Unknown JEDEC still clears retained session state without reading flash.
- Accept only a recognized full settings-record schema with a valid storage CRC and a valid PAN5 mapping. Reject duplicate/out-of-range IDs, invalid directions, unknown sizes, damaged data and unsupported schemas; preserve flash bytes.
- Reject project/preset save, erase, backup restore and sample/FM6-bank upload before mutations; verify UI/editor status as well as unchanged bytes. Live settings/calibration edits must clearly say RAM-only. Exercise autosave, idle flush and pre-reset hooks too.
- Internal snapshots use `JRM0`, work during the live session, and are cleared on reboot. A real Sloop parser must still accept its valid `FUN6` control and reject `JRM0`; JamBox rejects foreign project imports without changing the destination.
- App storage hooks refuse all writes; OTA hooks and the app's lowest driver admit staging only, `[0xE0000, 0xE5000)`. Normal boot is allowed to clean stale staging. The loader writes only the app and its known record sector; unknown flash causes no writes, and cleanup failures prevent a success reset while allowing retry in the current loader session.
- Exercise partial SysEx timeout/retry, reset during sending, bounded stalls and rescue queue draining through real transport/parser units. Retain ordinary Sloop tests and compile-time rejection tests for invalid JamBox configurations.

Compare the protected NOR bytes before/after, with raw mocks that actually mutate memory on unexpected writes. The ordinary Home test with storage stubs is insufficient for these contracts; use the real project/UI/editor/storage harness. The final evidence record retains initial failures and corrected reruns rather than reporting only the successful subset.

`SET3` settings and `PAN5` calibration remain shared retained layouts. If a warm return to Sloop finds no valid flash settings, it may retain temporary JamBox preferences/calibration. Test that transition during hardware preparation; the current project-family isolation does not imply every retained field is family-private.

JAM1 remains future implementation. Its separate acceptance cases are:

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

For the current SOUND/GROOVE increment, verify:

- GROOVE at startup; HOME toggles only while ordinary Home owns the gesture;
  return routes preserve the remembered view. Short HOME closes settings,
  a hold keeps its settings behavior, and release cannot add an unwanted toggle.
- Held-layer locks and both release orders, simultaneous HOME/PLAY,
  armed-record/count-in/free-take/confirmation overlays, and ordinary playback
  and overdub. View changes must preserve notes, patterns, transport and audio.
- Identical knob and keyboard behavior in both views, selected-part sound and
  macro values, correct unavailable-macro labels and browser control ownership.
- Literal 16-step rows for 1/15/16/17/32/64-step patterns, unequal part lengths
  and divisions, stored notes, ties, partial windows and disabled cells.
  Follow each part's playhead while playing; retain stopped windows, clamp
  length changes and reset genuinely replaced patterns without moving the
  editor cursor.
- Mute/solo/record distinctions, selected rows, zero-level/solo-excluded parts,
  cache invalidation, no stopped playhead, no stale fragments, readable actual
  renders and bounded LCD transfers using the existing strip buffer.
- Native/browser parity, updater-enabled target memory fit, package consistency
  and deployed byte checks. Owner audition remains a separate hardware check.

Compile the same C Assist/session/recording paths for native tests and WASM. Drive identical event sequences and compare recorded event bytes and audio within the existing fixed-point tolerance. Run the existing native/WASM test first, then add the JamBox cases; do not rely only on the checked-in upstream WASM.

Browser interaction must verify explicit audio start, 44.1 kHz enforcement or a tested resampler, panic on focus loss, no stuck keyboard/pointer notes, consistent part selection, audio-driven transport, stable performance during knob movement, and no device-MIDI access merely to play a local demo.

Walk the actual FM-1 control mapping with its 240×240 viewport. No required action may assume touch input, knob clicks or additional track buttons. Confirm the Assist menu is discoverable, selected-part/key scope is visible, combinations consume their component actions once, and macro labels match their sound.

User audition gate: make a beat, capture two chords, add bass and improvise a lead with little explanation. Compare the proposed Dusty palette to the owner's preferences. Adjust sound choice and parameter ranges based on listening, then build Garage/Neon.

## Gate 5: physical FM-1 bring-up

Before the first flash, identify the hardware/current Sloop version through read-only device information; export and validate a full backup; establish an available recovery procedure and equipment. This requires the owner and a physical device. It does not block software-only development.

First observe the currently installed, identified firmware as a baseline; select any subsequent firmware write from the exact supported transition and recovery plan. Confirm both normal/app-rescue return to the known-good package and a concrete ROM recovery route for failures before application rescue. If the chosen ROM tool requires a device-specific raw flash reference and an adapter, obtain and verify them before a trial; an editor backup alone does not establish that route. Use a charged device, stable direct data connection and known firmware package. Do not perform a deliberate power-cut experiment on the owner's only instrument without separate agreement and a demonstrated recovery route.

Check:

- Calibration, all keys/buttons/encoders, MASTER, display/LED behavior and original recovery gestures.
- Headphones/speaker output, gain, clipping, DC/clicks, note response and end-to-end latency.
- A ten-minute worst-case palette workload: bass, rapid four-note chords, lead, six drums, maximum exposed FX, active screen/knob input and USB recording.
- Total interrupt-duration histogram including nested timer/USB work: provisional p99 below 70% of deadline, maximum below 85%, zero late DMA halves and zero CPU-overload shedding. Distinguish intended voice stealing from overload shedding.
- Real stack guards, actual remaining ordinary/pool RAM, code size, NOINIT fit, and guard diagnostics.
- USB/TRS MIDI and sustain, USB audio continuity, and return through the supported updater/rescue path. The first temporary-session build must reject app-data saves; stopped/quiet saves, reload and dirty-state checks apply after JAM1 is implemented.

If the workload fails, reduce unused processing/buffers, shorten preset releases or choose a cheaper engine/FX combination. A triad fallback must be explicit and musically auditioned. Do not hide the overload indicator or simply increase voice counts.

## Gate 6: installer and release

Milestone 3's software is implemented: JamBox app/INFO/USB identity and splash, exact family probes in normal/rescue/loader modes, manifest-bound package validation, local build receipts, simulated transitions and read-only inventory. The candidate manifest keeps `release_enabled` and `hardware_validated` false and its approved physical-transition list empty. There is no public Install control or candidate firmware download.

Maintain separate checks for strict SysEx framing/length/checksum, JamBox family/mode/capability agreement, and the exact audited Sloop INFO profile. A numeric firmware version alone never establishes a family. Exercise both transfer stages, selected ports after re-enumeration, short final blocks including 481 bytes, invalid lengths, stalls, cancellation, disconnects and wrong final identity. Transition orchestration is confined to mock-only test helpers; the device page permits only identity/capability queries.

The current simulated matrix covers JamBox normal/rescue/loader to JamBox, exact Sloop 2.6 to JamBox, and JamBox normal/rescue back to the pinned Sloop 2.6 package. An unknown or standalone foreign loader is not a supported starting point. The test suite's synthetic package fixture proves host policy and transfer behavior; validate the actual built candidate separately. Source, flags, artifact, memory and canonical-container checks must pass before local candidate packaging. Keep firmware files outside the public site package and keep private reports/backups off the repository.

Physical validation remains pending. Verify each specifically selected transition, application rescue and return to Sloop on recoverable hardware before enabling installation. Complete the backup/recovery gate above first; simulation does not validate a backup or prove physical recovery. Do not generalize ChoralRoot's V15 recommendation into a proven universal reset.

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
