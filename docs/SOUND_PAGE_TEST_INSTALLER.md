# Sound-controls test installer

The normal installer includes the complete adopted Trial 05 drum editor:
https://anthonysprints.github.io/jamboxfm/install/

The separate sound-controls experiment is published only under:
https://anthonysprints.github.io/jamboxfm/preview/sound/install/

This candidate includes the full drum editor plus the new engine-specific
FM6 ENV/modulation cleanup described in [SOUND_PAGE_CLEANUP.md](SOUND_PAGE_CLEANUP.md).
It is not adopted into the normal release. Use Save now before either update;
manual saving and the current checkpoint format remain unchanged. Existing
users keep their storage setup; do not initialize it again.

Use the same recovery backup.json/full-flash-before.bin files kept for earlier
updates. They stay local to the browser and do not represent your current jam.
A successful installation response establishes firmware identity, not complete
hardware validation. Test SOFT PAD on Chords → ENV, its Time/Pitch controls,
return to an Analog sound and ordinary ADSR, blank modulation routes and hints,
recording, Home/held gestures, saved work and familiar drum editing.

## Packaging and publication

Dedicated tools bind exact firmware, clean compiled receipt, source archive,
static page/runtime, checksums and memory evidence. Publication is fixed to
`preview/sound/`, preserving every outside Git object/mode, including the
normal installer, browser instrument and saving/drum previews. Source commits
alone do not establish live publication.

```sh
. tools/cloud/activate.sh
python3 tools/package_jam_sound_preview.py --no-build
python3 tests/test_publish_jam_sound_preview.py
JAM_RECOVERY_SITE_ROOT=build/jambox-sound-preview JAM_RECOVERY_EVIDENCE_DIR=build/jam-sound-install-evidence node tests/jam_recovery_install_browser.mjs
python3 tools/publish_jam_sound_preview.py
python3 tools/publish_jam_sound_preview.py --publish
```

The existing validated sound-cleanup target may be retained/reused if its
firmware remains SHA256
`b3f31704d063f63d79b8019b5e166e2677bf39df9e9a9b4bddb86a7a887ff99a`
and the stock source/ELF/generated-header checks still pass. Installer-only
changes do not require replacing that tested application. A future source
change requires a new build and validation; do not substitute unrelated bytes.

Publication evidence belongs in `docs/build-evidence/sound-page-installer/`.
Remote Git bytes, raw HTTP and live Pages availability are checked separately.
Physical FM-1 acceptance of the sound-page experiment remains pending.
