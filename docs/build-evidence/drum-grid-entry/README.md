# Direct drum GRID entry — October 15, 2026

First implementation stage of issue #5 and the drum workflow plan. The source
revision in status.json produced the target and its corresponding source bundle.
The change adds direct SEQ entry on Drums, immediate keyboard-mode publication,
and recorder guards; it adds no DSP engine, save format or automatic writer.

The real shared UI fixture reproduces 91 navigation/recording/modal checks and
passes GCC ASan/UBSan. Native UI rendering asserts LCD rectangle bounds. Three
Chromium scenarios execute the production panel, AudioWorklet and C renderer;
the existing full emulator regression also passes. All 30 isolated installer
Chromium cases pass, including complete transfers and privacy/error guards. Two
saving-default controller cases also pass. Native manual recovery and
Home tests pass. All source/ELF/compiler/SDK/canonical-package gates passed for
the actual FM-1 candidate, with the reported linked resource reserves.

The new fixed-prefix publisher protects every object/mode outside preview/drums/.
Local bare-remote tests cover containment, races, dry run/no-op, validation and
symlinks. Remote Git and raw HTTP checks are recorded separately after actual
publication. A successful repository push alone does not establish Pages
reachability or a physical FM-1 trial. No private backup/report is published.

The firmware family identity is shared with earlier JamBox builds. The actual
feature check is Drums → SEQ opens GRID. Owner feedback remains pending before
promoting this candidate to the normal installer. Keep main, normal installer,
emulator and the saving preview unchanged during this isolated trial.

See [trial instructions](../../DRUM_GRID_TRIAL.md) for exact controls, limitations,
maintainer commands and the physical workflow check.

Independent remote verification confirms all 29 package files match and all 89
protected Git entries/modes are unchanged. Five raw GitHub HTTP artifacts match
the staged hashes. Live Pages HTTP remains blocked by this cloud proxy with
CONNECT403, so live reachability is not claimed.
