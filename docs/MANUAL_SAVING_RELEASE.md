# Manual saving in the normal Chrome installer

On October 15, 2026, the owner confirmed that Save now survives full FM-1 power-off
and restores the jam and edited sounds. They explicitly approved offering that
same firmware through the normal Chrome installer, after promoting the source
to `main`. This records owner feedback, not an independent hardware test.

The approved firmware is 611,255 bytes, SHA-256
`b57fcb452c05933b5aa895840067d18db10f70ee3f70c8d35e15bf760e10f4c7`.
Release preparation must compare the newly built package to that exact tested
file. Its app is 574,572 bytes; ordinary RAM 91,432 bytes and pool 329,932 bytes.
These linked budgets retain their existing reserves; they do not measure peak
stack or physical deadlines. Broader recovery scenarios remain separately tested
in host models and must not be described as fully verified on hardware.

## Chosen behavior

- Save now is an intentional checkpoint. Restart returns to the last complete
  manual save; experimenting without saving does not overwrite it.
- Automatic saving is disabled and deferred by the owner.
- First use requires the explicit on-device storage warning, backup acknowledgement
  and fresh two-second SAVE hold. Initialization replaces legacy projects/USER
  sound allocations, preserving the protected areas and factory sounds.
- The current jam, populated A–D, arrangement, edited FM6 patches, shared SYN
  kits, selected view/track/octave and supported preferences are included.
  Named archives and additional tracks are separate work.
- Existing saving builds should save before updating. Earlier temporary builds
  lose their current RAM jam when updating; the installer explains this.

The normal updater build now uses `JAMBOX_RECOVERY=1`, `FELUCCA_PERSIST=0`, FLASH
and OTA enabled. Other inherited persistence writers remain disabled. The normal
packager requires this profile; the separate development bundle retains its own
manifest and URL. The low-level default remains recovery off for unrelated
diagnostic/browser builds. Browser sessions remain temporary.

## Build, check and publish

Use the existing checkout and activate its retained tools:

```sh
. tools/cloud/activate.sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/package_jam_installer.py --no-build
python3 tests/test_package_jam_device.py
python3 tests/test_package_jam_recovery.py
python3 tests/test_publish_jam_installer.py
JAM_INSTALL_SITE_ROOT=build/jambox-installer node tests/jam_install_browser.mjs
python3 tools/publish_jam_installer.py
python3 tools/publish_jam_installer.py --publish
```

Package after source changes settle. The installer bundle verifies exact source,
compiler/SDK provenance, ELF/resources, canonical firmware format, receipts,
runtime bytes and notices. The firmware and source downloads use the existing
normal URLs. The installation transport and factory-V15/JamBox identity guards
are unchanged. Whole-device `hardware_validated` remains false because one
owner's successful save/restart does not validate every device/recovery behavior.

The dedicated normal-installer publisher updates only installer/runtime files,
the firmware bundle and corresponding source. It merges the root checksums and
preserves every other Git object and mode, including the browser instrument and
`preview/saving/`. Default is a dry-run; explicit publication is non-forced and
rejects remote races. The old full-site publisher would replace unrelated pages
and must not be used for this scoped release.

After publication, independently verify the remote changed-path allowlist and
all published artifact hashes. Repository/raw HTTP checks and live Pages
reachability are separate: the cloud proxy previously blocked the Pages domain
with CONNECT403 despite its saved allowlist. Do not claim live reachability
without evidence. The owner already confirmed that the separate preview opens.
