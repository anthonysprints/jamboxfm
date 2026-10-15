# Adopted drum workflow in the normal installer

On October 15, 2026 the owner requested that all drum work, including Trial 05's
individual synthesized-drum editor, be promoted to main and the normal Chrome
installer. See [owner adoption](build-evidence/drum-sound-release/owner-feedback.md).

The adopted firmware is exactly 611255 bytes, SHA256
`a88970d9e601aed4042e28302d9ada19357b4ac549859a479f8eee5fba04b351`.
Its original owner-used clean source is `4c04594`; the drum branch/evidence tip
was `0e69bbd5fdb56e0739d8ab1155853ce161d20cf8`. Promotion must preserve those
exact firmware bytes. A fresh target rebuilt at the drum tip is acceptable only
when its hash is identical and the stock ELF/source/container gates pass.

## Included behavior

- SEQ opens the named eight-row drum grid; repeat SEQ swaps drums 1–8 / 9–16.
- Grid knobs control Drum / Step / Strength / Repeat.
- EDIT shapes the whole kit; FX exposes useful kit effects and retained depth.
- ENV selects and edits individual synth-drum Tune / Decay / Filter / Level.
- Factory recipes require an explicit copy into a chosen SYN1–4 bank. The
  confirmation warns of whole-bank replacement and shared A–D section use.
- Closed/pedal hats indicate their linked recipe. Sampled kits use EDIT/FX.
- Empty sample banks are skipped; factory and valid SYN kits remain available.
- Home-aligned left knobs, held gestures, recording overlays, first-key playing
  and intentional manual saving remain intact.

Automatic saving stays off. Existing checkpoint identity/layout are unchanged;
Save now keeps the edited SYN banks and jam. Users should Save now before
updating and keep their existing saving setup. First-time factory users retain
the explicit initial storage setup. No new tracks or multiple-song slots ship.

## Promotion and isolation

Main before Trial 05 promotion was `67cb6ee2e7dd05b1911b90b8ab92696a347b1e51`.
It fast-forwards to the drum tip; normal-installer documentation can follow.
Keep `next/sound-page-cleanup` and its FM6/modulation UI separate. The new test
installer belongs at `preview/sound/`; no sound-cleanup code belongs in this
normal release until separately adopted.

The normal scoped publisher preserves the browser instrument, existing drum
and saving previews, and every unrelated Git path/object/mode. A subsequent
sound-preview publication must in turn preserve this normal release. Do not use
the full-site publisher. Current evidence belongs in
`docs/build-evidence/drum-sound-release/`.

```sh
. tools/cloud/activate.sh
python3 tools/check_jam_recovery_identity.py
python3 tools/package_jam_installer.py --no-build
python3 tests/test_package_jam_device.py
python3 tests/test_package_jam_recovery.py
python3 tests/test_publish_jam_installer.py
JAM_INSTALL_SITE_ROOT=build/jambox-installer node tests/jam_install_browser.mjs
python3 tools/publish_jam_installer.py
python3 tools/publish_jam_installer.py --publish
```

Use the normal schema-2 install descriptor and normal candidate manifest, not a
renamed preview manifest. Verify firmware hash before/after publication, remote
files and protected trees. Owner adoption is distinct from universal hardware
validation. Software markers remain unchanged. A source/site push alone is not
proof of live Pages availability; record HTTP access independently.

## Earlier release

Trial 04 was adopted earlier that day with firmware SHA256
`77812285810b5c0494d94783d63d49c747e5e918c3815a689c66f4fe67e3cb90`,
clean source `d41e979dd150f5893cc8b5719b58fe5b0f28b7cb`. Its evidence remains in
[drum-release](build-evidence/drum-release/). Trial 05 adds the individual drum
editor while preserving that tested grid/kit workflow and manual-save format.
