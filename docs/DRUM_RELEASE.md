# Adopted drum workflow in the normal installer

On October 15, 2026, the owner accepted Trial 04 and explicitly requested moving
all drum changes to public main and the normal Chrome installer. This follows
the earlier manual-saving adoption. See [owner acceptance](build-evidence/drum-seq-banks/owner-feedback.md).

The approved firmware is exactly 611255 bytes, SHA256
`77812285810b5c0494d94783d63d49c747e5e918c3815a689c66f4fe67e3cb90`.
Its clean compiled source is `d41e979dd150f5893cc8b5719b58fe5b0f28b7cb`.
Promote those exact bytes; do not substitute an untested rebuild. The normal
filename/manifest differ from the preview, while the firmware bytes remain
identical. The source archive includes the release documentation and normal
installer HTML, with compiler inputs bound to the original receipt.

## Included behavior

- Direct SEQ entry, eight named rows, explicit step windows and useful
  Drum / Step / Strength / Repeat knobs.
- Repeated SEQ swaps drums 1–8 / 9–16 while preserving row position and steps.
- EDIT opens KIT SOUND; FX opens useful whole-kit effects and retained depth.
- Empty user sample kits are skipped; factory and valid SYN kits remain.
- Ordinary left knobs agree with Home; held gestures, recorder priority,
  manual saving and immediate keyboard ownership remain intact.

Manual saving is unchanged, automatic saving stays off, and existing checkpoint
identity/format remain compatible. Existing users should Save now before
updating and keep their saving setup. First-time factory users still follow
the explicit initial storage setup. No new tracks, song slots, automatic save,
individual drum recipe editor or additional flash writer is included.

## Promotion workflow

Main before adoption is `da394a87b3345555e796257012e7adf4bbbc8872`; it is an
ancestor of the drum branch. Fast-forward main to the reviewed work without
rewriting history. Future unapproved experiments remain isolated. The public
browser instrument and both preview trees are preserved by this installer-only
publication.

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

Reusing the verified Trial 04 target is intentional. The normal packager checks
its actual source/generated inputs, compiler/SDK provenance, ELF memory,
canonical container and loader. Normal --with-updater and development
--with-recovery use the same approved flags. A future fresh build uses
`bash tools/cloud/build_device.sh --with-updater`; compare bytes to the tested
hash before substituting it into this release.

Verify that normal firmware SHA256 equals the owner-tested hash before and
after publication. Use the normal schema-2 install descriptor and normal
candidate manifest, never a renamed preview manifest. The scoped publisher
updates only its exact installer/source/firmware allowlist and merged root
checksums, preserving every other Git path/object/mode. Do not use the full-site
publisher. Verify new remote bytes, unchanged protected trees and main history.

Owner acceptance is distinct from independent or universal device/recovery
validation. Existing software-only policy markers remain unchanged. The staged
trials record native, audio, sanitizer, browser, target and save-compatibility
checks. Current promotion checks and publication/HTTP outcomes belong in
`docs/build-evidence/drum-release/`. A successful push is not proof of live
Pages HTTP availability; report a cloud proxy failure separately.
