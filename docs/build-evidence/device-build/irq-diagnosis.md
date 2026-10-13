# Sloop baseline IRQ budget investigation — 2026-10-13

**The IRQ-budget mismatch is inherited from the published Sloop 2.6 image and its older reference budget. The supplied compiler did not introduce a new IRQ instruction-path increase in our unchanged baseline rebuild.** The budget check still fails and has not been changed or bypassed. This is a stale upstream reference discrepancy to resolve with an explicit review, not proof that either firmware meets physical audio deadlines.

## Direct binary evidence

Unchanged source: `isod89/sloop-fm1` commit `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`. Compiler: owner-provided `jieli-linux-toolchains-20260730.1`, archive SHA-256 `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`; vendor Clang reports 4.0.1. Local archive hashing pins supplied bytes; it is not a vendor signature.

Decoded the repository's tracked `docs/firmware/sloop-2.6.fwsc` offline, validating the UFW header/list, flash image, ISD entry and application entry/data CRCs. Compared its decrypted application to the newly built `build/felucca.bin`:

- All **573,052** rebuilt application bytes match except **one byte**, at app offset `0x8A2E2`: the displayed `__DATE__` string changes from `Oct 11 2026` to `Oct 13 2026`. Source: `firmware/src/ui_menu.c:95`.
- All remaining published app-slot padding is `0xFF`.
- `fm1_alnk0_irq`, address `0x0200CFEC`, is **710 bytes, byte-identical** in both images.
- Both IRQ byte sequences have SHA-256 `dbdc6b5469ab585136d7899d583da4366406e1c3a2d5b0a3cbd4dcebff394c3d`.
- Published `.fwsc` SHA-256: `199829bb5b892feae9058158dfed7a137c83ec4d8dfc1223761be1f5614ad6a2`.

This establishes equivalence to the pinned published app's machine code, apart from its build-date text. It does not authenticate an upstream hardware test or prove the entire firmware package is byte-identical. The full evidence and reproducible extraction/comparison are in `irq-history/published-comparison.json` and `irq-history/compare_published.py`. No extracted firmware needs to be redistributed with these text reports.

## What the failing check actually measures

`tests/target_budget.py` parses instruction-address lines until the next function label, finds backward branch spans, and weights instructions within nested spans by powers of four. Divides are weighted by nine. It reports call counts but does not include callees' full execution time. This is a reproducible **static heuristic**, not cycles, deadline duration or actual CPU load.

| Build | IRQ instructions | Instructions in loop spans | Weighted cost | Stored budget | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| Unchanged Sloop 2.6 rebuild | 228 | 91 | 268 | 174 | Fails +10% allowance; +54% |
| Current JamBox diagnostic | 225 | 88 | 256 | 174 | Fails +10% allowance; +47% |

The baseline's six detected backward branches form three nested pairs present in its actual disassembly: two pairs in the conditional overload voice-shedding path and one in block/sample output processing. Their weighted contributions are 57 + 132 + 79 = 268. JamBox has the same first two contributions and 67 for output processing, totaling 256. No misidentified extra function or unrelated trailing label explains the result. The heuristic combines conditional paths and does not measure how often they run.

The budget file labels its earlier baseline “SLOOP 2.0 (reviewed)” and still stores IRQ cost 174. Its later Sloop 2.4 comment explicitly leaves three FM6 functions without budgets because that port had been made without the target compiler. The read-only GitHub path-history query returned two changes: Sloop 2.0 (`a3f9fd0`, October 4) and Sloop 2.4 (`8d3823f`, October 7), with no subsequent update through pinned 2.6. We did not recreate the older build behind 174, so do not attribute a precise older instruction change or compiler version to it.

Evidence: `irq-history/irq-cost-breakdown.json`, `irq-history/budget-history.json`; source permalinks:

- [Estimator and its stated limits](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tests/target_budget.py#L4-L11).
- [Actual loop-cost extraction](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tests/target_budget.py#L34-L75).
- [Reference budget and historical comments](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/tests/target_budget.txt#L1-L24).
- [IRQ source](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/audio.c#L28-L94).
- [Build-date display](https://github.com/isod89/sloop-fm1/blob/8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e/firmware/src/ui_menu.c#L95).

## Full baseline-suite outcome retained

`sh tests/run_tests.sh` exited **1**. Two checks failed:

1. The IRQ static budget described above.
2. The final PWA browser test found Playwright, but neither its hard-coded `/opt/pw-browsers/chromium` nor its default cached Chromium executable was available. Launch failed at `tests/pwa_test.mjs:42`. This is a browser-path/environment failure; it is not a firmware failure or an optional-test skip.

The preceding native/simulated tests completed successfully, including flash storage, rescue/bootguard, sequencer/UI, the runner’s scoped sanitizer checks, ten minutes of simulated random use, OTA staging, loader retries/bounds, installer and web protocol simulations. All 182 golden renders were unchanged; health and voice/routing checks passed. The timing-based host CPU check reported 19 advisory notes; there was no host instruction counter, and these notes do not validate target timing. Three FM6 target functions have no inherited budget. The full failed run is retained as `baseline-tests.log` / `baseline-tests.exit`; no baseline/reference files were changed.

## Exact local reproduction

From the existing unchanged checkout, with the imported SDK/compiler:

```sh
cd /workspace/scratch/fm1-research/sloop
. /workspace/jamboxfm/tools/cloud/activate.sh
git rev-parse HEAD
git status --short
bash build.sh
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
```

The final command is expected to exit 1 for the preserved IRQ mismatch. For the binary comparison after a build:

```sh
/workspace/toolchains/jieli/toolchain/common/bin/objdump -t \
  /workspace/scratch/fm1-research/sloop/build/felucca.elf \
  > /workspace/scratch/fm1-device-build/baseline/app-symbols.txt
python3 /workspace/scratch/fm1-device-build/baseline/irq-history/compare_published.py \
  --repo /workspace/scratch/fm1-research/sloop \
  --symbols /workspace/scratch/fm1-device-build/baseline/app-symbols.txt \
  --out /workspace/scratch/fm1-device-build/baseline/irq-history/published-comparison.json
```

On a different date, the number of build-date text bytes may differ; the comparison records every differing byte. The script performs no MIDI, network or device I/O. Full original-suite reproduction is `sh tests/run_tests.sh`; the separate PWA rerun is recorded in `pwa-runtime-notes.md`, and reviewing the stale budget remains separate. No budget update is implied by this report.
