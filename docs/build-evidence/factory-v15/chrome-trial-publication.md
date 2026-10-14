# Experimental Chrome installer publication

October 14, 2026. Implementation source commit `06b9391`; the owner requested
installation after completing the verified private factory backup. No cloud
command accessed the physical FM-1. Physical installation remains the owner's
next action in Chrome.

## Published build

- Candidate: `jamboxfm-0.1.0-dev.fwsc`, 611,255 bytes.
- SHA-256: `8bc79f41770b54de2af863aa1dcfd6ed7c80ddb962c92cd582178697038f5c42`.
- App: 547,552 bytes; ordinary RAM 80,648/98,304; pool 324,580/344,064.
- Loader: 9,368 bytes; OTA container 8,099 bytes; all ELF/memory gates pass.
- Manifest keeps hardware/release flags false. Separate `trial.json` binds
  exact bytes to the experimental factory-V15-to-JamBox controller.

The build receipt records a dirty source snapshot before the implementation
commit. Its input hashes match the committed firmware/build files; the
downloadable corresponding source contains those exact inputs. This is an
input-hash receipt, not a signed release or a claim that the receipt names the
later commit. Package reconstruction, archive hashes and source/receipt
consistency were checked before publication.

The [stock record correction](stock-record-cleanup.md) adds validated E8F00
handling while retaining the E4F00 custom path. Programming stays within the
app region. The old unsupported interpretation of the PI32 parallel header
store was corrected before the final build; only staging-address-at-+4 is
accepted. A build during source review was refused by the changed-input guard
and replaced by the successful final build. No failed/stale build was published.

## Completed software checks

| Check | Result |
| --- | --- |
| Exact V15 to final candidate, native ASan/UBSan loader tests | 47 legacy / 107 JamBox checks passed |
| Real JamBox factory-return staging, native ASan/UBSan | 34 checks passed |
| Storage, calibration, write-boundary and recovery regression runner | Passed |
| Candidate-packaging and ELF/memory unit tests | 12 + 12 passed |
| Shared browser transport and existing device-policy tests | 36 passed |
| Production trial controller with final candidate | 14 passed |
| Final candidate integration | Nine simulated transitions, 17 transfer stages; every logical byte checked |
| Existing read-only device page, Chromium | 18 cases passed; outbound read-only allowlist checked |
| Final packaged installer, Chromium | 18 cases passed, including full transfers, identity failures, duplicate/hotplugged ports, cancellation, interrupted writes, report privacy and mobile layout |
| Publication validation | 14 tests and real Git push dry-run passed |

Activated browser Clang lacks its sanitizer runtime archives. The factory
sanitizer runner was rerun with `CC=/usr/bin/gcc`, retaining both sanitizers
and fail-on-error flags; it passed. No sanitizer assertion was disabled.

The inherited static IRQ estimate still fails 256 > 174, versus 268 in the
published Sloop 2.6 baseline. Its threshold remains unchanged. Physical audio
timing, installation, rescue/return and interrupted-install recovery remain
untested; these software results do not promote hardware-validation flags.

## Deployment and live verification

The normal non-forced publisher deployed exactly 50 packaged files as
`02a743e04a7cbce16fef21114b1d88e6113fc75a` on `gh-pages`, preserving its parent
`c07e0f4cebc4f790c03cddae9df0e6a5db824c29`.
`SHA256SUMS` SHA-256:
`550b4fa6ba3eb8d80137ce46049795270987e721f4517384946da23e81d9d853`.

[GitHub Pages run 38512981699](https://github.com/anthonysprints/jamboxfm/actions/runs/38512981699)
completed successfully for that exact deployment commit. All 49 public files
were then downloaded over HTTPS and compared byte for byte with the package;
all matched. The excluded fiftieth entry is the `.nojekyll` configuration
marker. This includes firmware, corresponding source, all installer modules,
the emulator and its WebAssembly, the read-only checker, licenses and manifests.

Live installer: <https://anthonysprints.github.io/jamboxfm/install/>.
The new repeated backup uploads were byte-identical to the already verified
set. Neither upload set, manufacturer firmware nor device reports were included
in the source or website. Reserved inherited artwork/deployment files remain
untouched and untracked.

Reusable cloud startup notes were updated in the environment configuration
draft. Saving the draft does not publish/apply that environment configuration.
