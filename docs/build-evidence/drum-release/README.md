# Owner-approved drum promotion — October 15, 2026

The owner accepted Trial 04 and explicitly authorized all drum changes on main
and the normal Chrome installer. See [owner feedback](../drum-seq-banks/owner-feedback.md)
and [release notes](../../DRUM_RELEASE.md).

Main before adoption is da394a87b3345555e796257012e7adf4bbbc8872. The complete
reviewed drum branch is a descendant, so source promotion is a fast-forward
with no rewritten history. Release documentation, normal-installer copy and a
generic publishing commit label are the only subsequent source changes.

The normal installer uses the exact owner-tested 611255-byte firmware SHA256
77812285810b5c0494d94783d63d49c747e5e918c3815a689c66f4fe67e3cb90.
It is byte-identical to the Trial 04 download and retained compiled target.
No firmware rebuild or codec/runtime/save-format change was made. The normal
schema-2 descriptor and normal candidate manifest are regenerated with current
corresponding source and the original clean d41e979 build receipt.
The current source package includes release docs and installer HTML; compiled
input hashes match that receipt. Native/audio/browser/device checks recorded
for the exact tested binary remain applicable. Automatic saving stays off.

The normal package/provenance/memory/canonical-container checks pass. Thirteen
device package, nineteen recovery package and seven scoped normal-publisher
unit tests pass. Full current normal installer browser outcomes and independent
main/site publication checks are recorded below and in status.json. The public
browser instrument and both preview trees are protected by the normal publisher.
No user backup, device report or manufacturer firmware is included here.

All 23 Chromium normal-installer cases pass against the exact repackaged
candidate. They cover complete factory-to-JamBox and JamBox-to-JamBox two-stage
byte transfers, fresh source identities/capabilities, broken descriptors,
permission/disconnect/final-release failures, local report privacy, and desktop
and mobile layout. These are transport simulations, not new device flashing.

Source promotion was pushed to main at
1e4512cede4c8d34ee2367c049925a5d788445e1. Normal-installer publication is
8b19873b37021f3e6848f6b9dc9127d1769e7d01, on top of the retained Trial 04 site.
The scoped publisher preserved 90 unrelated entries/objects/modes. Independent
remote and HTTP verification outcomes follow in the evidence JSON files.

Independent verification passes: remote main contains the prior main, clean
tested source and complete trial history; the website commit/tree matches its
report. Only 10 allowlisted paths changed. All normal package bytes and merged
root checksums are verified, including 49 valid checksum entries and retained
unrelated entries. All 90 protected objects/modes, the public emulator and both
preview subtrees are identical to before publication. Five pinned raw GitHub
HTTP requests returned 200 with exact bytes. Direct live Pages access remains
blocked from this environment with CONNECT403; live reachability is not claimed.
