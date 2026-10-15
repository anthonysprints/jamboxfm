# Normal manual-saving release — October 15, 2026

The owner confirmed Save now survives full power-off and restores the jam and
edited sounds, then authorized promotion to main and the normal Chrome installer.
Automatic saving stays disabled. The firmware is byte-identical to their tested
preview; an owner already using that preview does not need to reinstall.

The clean source revision in status.json produced the published archive and build
receipt. Subsequent evidence-only commits do not change that provenance. The
normal profile enables JAMBOX_RECOVERY=1 with FELUCCA_PERSIST=0; this release makes
no C runtime changes. Browser sessions remain temporary.

The scoped publisher updates 29 package paths and protects all other tree entries.
Remote verification compares published bytes, all root checksums, and the unchanged
emulator/preview Git objects. Raw HTTP and live Pages results are recorded
separately. Publication alone does not establish live Pages reachability.

The 23-case Chromium installer run covered the final installer runtime and exact
firmware; the subsequent clean rebuild changed only device-checker wording and
source provenance. The final device-checker passed all 18 cases, and the production
bundle loader accepted the final descriptor/manifest/firmware hash bindings.

See [release procedure](../../MANUAL_SAVING_RELEASE.md) for reproduction commands.
Private device backups and reports are excluded from this evidence.
