# Quick SEQ drum-bank shortcut — trial 04

October 15, 2026. The owner reported that Trial 03 looked good, found SEL/ENV
hint wording confusing, and requested an easier jump to the second eight drums.
The hints were explained; their functions and text are unchanged in this update.
See [Trial 03 feedback](../drum-kit-controls/owner-feedback.md).

Inside ordinary GRID, another SEQ tap selects the corresponding row in the
opposite drum bank (1–8 / 9–16). A second tap returns. The selected step and
16-step window stay fixed. From another screen, SEQ opens GRID without changing
the selected drum. Held SEQ and recording overlays retain ownership. Navigation
does not audition, change the beat/mix/undo, start recording or write flash.

The production change is two lines in ui_input.c. Source is
`d41e979dd150f5893cc8b5719b58fe5b0f28b7cb`; the target receipt records clean
source. Firmware SHA256:
`77812285810b5c0494d94783d63d49c747e5e918c3815a689c66f4fe67e3cb90`.
The app uses 577312 bytes, leaving 4252 FLASH bytes; RAM and pool remain
91432 and 329932 bytes. Target/ELF/package/loader checks pass without warnings.
The saved runtime identity, reviewed catalog digest, assets, codec, save format
and writer remain unchanged. Automatic saving stays off.

1360 native GRID assertions cover every drum, lengths 1/17/32/64 and all valid
step windows, including two-tap return, exact beat/mix/undo preservation,
held-step release, first key after a bank jump, and playback/overdub continuity.
All pass under GCC ASan/UBSan too. The 92 navigation, 336 KIT SOUND/FX and
45 save-modal checks pass. Six Chromium scenarios use the production worklet,
panel, audio and renderer; the added scenario checks silent switching, exact
LCD return after two taps, independent drum edits on step 18, remembered entry
from Home/KIT/FX and uninterrupted playback. Both native-size bank screens were
visually inspected. The browser runner returned exit code 0; no stdout log was
retained for this run. Screens and the executable fixture are retained.

Previous Trial 03's full 29-file package was retained and validated locally; its
pinned website commit acb8782a6b30c31ec6cfeb0226e3fd5478cd3df8 remains the public
return reference. Main, the normal installer, public emulator and saving preview
must remain unchanged. New installer/publication outcomes are recorded below
and in status.json. The owner's physical Trial 04 check is still pending.

Two focused Chromium installer cases pass against this exact new package: the
complete two-stage transfer and private report, plus mobile layout/local backup
controls. The unchanged controller's full 30-case Trial 03 run is recorded in
the preceding trial evidence; it was not rerun in full for this two-line update.

Published as `e375547c077073fa37389e84cdb4539c05bb0a59` only below preview/drums/. Independent Git
verification confirms every package file, all 89 outside objects/modes, and
unchanged main/normal installer/emulator/saving preview. Five pinned raw GitHub
HTTP artifacts match. Live Pages access is blocked by the cloud proxy with
CONNECT403; availability at the Pages URL is not independently claimed.
The installer page identifies this candidate as Trial 04.
