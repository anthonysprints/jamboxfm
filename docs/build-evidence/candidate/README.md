# JamBox identity, local candidate and read-only device check

Milestone 3 completes the software preparation for a first FM-1 trial. The candidate is local only: `release_enabled` and `hardware_validated` are false, with no approved physical transitions. No physical FM-1 was connected or flashed. The public package contains the browser instrument and read-only `/device/` page; it contains no firmware download or Install action.

Starting source was `0d587ef11bd9ed7b95210e6ff666fb404dcbce10`. The [build receipt](build-receipt.json) records the changed working-tree compiler inputs, generated headers, actual preprocessed flags, pinned compiler/SDK and artifact hashes. Its dirty-source flag is intentional; the starting commit alone does not identify this implementation. The receipt is not a cryptographic signature. Final code and test inputs are recorded in [source hashes](source-inputs.sha256).

## Recovery and corrected defects

The interrupted work was snapshotted before edits. Review caught a 48-byte family reply exceeding the inherited transmit buffers, an incorrect assumed Sloop INFO profile, preprocessor expansion corrupting receipt flag names, a missing drum-kit generator input, and a simulator missing port-state listeners. These were corrected before the final checks. Cancellation waits for pending port cleanup; unrelated MIDI output ports are no longer selected as a fallback. Python confirmation interruption closes ports, and JamBox candidate refusal happens before creating a MIDI backend.

JamBox now reports `FM-1_700`, loader `ota-FM-1_700`, and family `JAMBOXFM`; rescue retains `FM-1_000` plus the exact family probe. Published Sloop 2.6 is audited as `FM-1_900` / `FELUCCA SLOOP 2.6`. A number alone does not establish a firmware family. See [the protocol and workflow](../../FM1_CANDIDATE.md) for exact contracts and remaining gates.

## Executed checks

| Check | Result | Evidence |
| --- | --- | --- |
| Updater-enabled pi32v2 target and local packager | Exit 0; canonical package, tool/input/artifact checks and memory gates pass | [Build](device-build.log), [package](package.log), [memory](memory.json) |
| Build from extracted corresponding-source ZIP, without Git metadata or reserved artwork | Exit 0; app, loader, OTA, package and both ELF files byte-identical; clean package verifies with null Git provenance | [Build](clean-build.log), [package](clean-package.log), [comparison](clean-artifacts.json) |
| Identity, OTA query bounds and USB names in legacy/normal/rescue/loader modes, plus actual splash renderer | Nine ASan+UBSan binaries pass | [Log](identity.log), [splash](jam-splash.png) |
| Python strict-wire/port/cleanup regressions | 17 tests pass | [Log](python-wire.log) |
| Existing Python installer simulations | Exit 0 | [Log](python-install.log) |
| JavaScript strict transport and read-only query allowlist | 18 tests pass | [Log](web-transport.log) |
| Manifest, family and simulated-transition fault cases | 9 tests pass, including six transitions and adversarial subcases | [Log](device-policy.log) |
| Actual candidate bundle and exact pinned return package | Hash/size verification and six actual-byte mocked transitions pass | [Log](actual-candidate.json) |
| Package refusal and memory report gates | 12 plus 12 tests pass | [Package tests](package-tests.log), [report tests](report-tests.log) |
| Existing temporary-session storage, calibration, OTA, USB, recovery and loader regressions | Exit 0; real code over simulated hardware | [Log](storage-regression.log) |
| Existing browser instrument | 36 native checks plus real Chromium audio/control/desktop/mobile/standalone tests pass | [Log](emulator.log) |
| Packaged read-only device page | 17 Chromium scenarios pass; every transmitted packet matches the query allowlist | [Log](device-browser.log), [screen](device-page.png) |
| Existing web editor/package/installer simulations | Exit 0 | [Log](legacy-web.log) |
| Publisher using local bare Git remotes only | 7 tests pass, including firmware exclusion and corresponding-source mismatch refusal | [Log](publisher-tests.log) |
| Static audio IRQ instruction-cost budget | **Exit 1**, unchanged estimate 256 versus inherited budget 174 | [Log](target-budget.log) |

The inherited full Sloop runner was not repeated here. Its known Chromium PWA installability failure and previous DSP signed-shift sanitizer findings remain recorded in earlier evidence. No budget or golden baseline was relaxed. Identity-specific sanitizer success is not a claim that the entire inherited DSP is sanitizer-clean. The existing web/Python installer suites ran against the pre-candidate package; the actual-candidate and storage checks use the new package identified below.

## Final target

| Region | Used bytes | Capacity bytes | Spare bytes |
| --- | ---: | ---: | ---: |
| Application image | 547,552 | 581,564 | 34,012 |
| Ordinary static RAM | 80,648 | 98,304 | 17,656 |
| Large-buffer pool | 324,580 | 344,064 | 19,484 |
| RAM code | 2,784 | 24,576 | 21,792 |
| Retained NOINIT | 15,464 | 15,696 | 232 |
| Loader region span including alignment and BSS | 19,856 | 81,920 | 62,064 |

The loader image is 8,600 bytes; its compressed OTA is 7,309 bytes. Its 7,277-byte body is below the 19,968-byte body limit. These figures describe linked sections, not measured stack or audio timing. NOINIT remains tight.

Candidate: 610,465 bytes, SHA-256 `ab0c2071c2a3e18ce6c8ee7a55621af8f20aa1fe978c1784481352542fc2a0b2`. Exact external Sloop return fixture: 610,019 bytes, SHA-256 `199829bb5b892feae9058158dfed7a137c83ec4d8dfc1223761be1f5614ad6a2`. Neither binary is committed or included in the public site. Reproducibility is same-day; the inherited `__DATE__` can change later builds.

## Reproduce

From the existing checkout:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/package_jam_device.py --no-build
sh tests/jam_identity_run.sh
python3 tests/jam_install_wire_test.py
python3 tests/test_package_jam_device.py
python3 tests/test_report_device.py
node --test tests/jam_web_transport_test.mjs tests/jam_device_policy_test.mjs
node tests/jam_device_candidate_check.mjs
sh tests/jam_storage_run.sh build/felucca.fwsc
sh tests/run_jam_emu_tests.sh
JAM_DEVICE_SITE_ROOT="$PWD/build/jambox-web" node tests/jam_device_browser.mjs
python3 tests/test_publish_jam_web.py
python3 tests/target_budget.py
```

The final command preserves the known failure. The transition checks require the separately cached exact Sloop package; pass its path as the second argument to `jam_device_candidate_check.mjs` or use `JAMBOX_SLOOP_PACKAGE` for the policy suite. The fixture is not redistributed. Tests use simulated MIDI endpoints, never an attached instrument. Publication is a separate explicit step in [GitHub Pages instructions](../../GITHUB_PAGES.md).

Next: read the owner's actual firmware identity, verify available private backups and recovery equipment/procedure, then plan the physical trial. Simulated return to Sloop does not prove preservation of data on real hardware.
