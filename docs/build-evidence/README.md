# Prototype build evidence

Executed October 13, 2026 in the Debian 13 amd64 cloud environment. The source baseline is Sloop `8fad1a15ff59d5c3ed0bfe2d7a9ae90962cf4d5e`; original import hashes are in [the import manifest](../upstream/import-manifest.json).

The later FM-1-style web emulator has its own [test, package and source-rebuild record](emulator/README.md). The results below identify the original four-part browser workbench.

| Check | Result | Log |
| --- | --- | --- |
| Unchanged `python3 tools/build_preview.py` | Exit 0 | [Build](baseline/sloop-build-preview.log) |
| Unchanged `sh tests/run_preview_tests.sh` | Exit 0; all 12 engines native/WASM exact | [Baseline tests](baseline/sloop-preview-tests.log) |
| `bash tools/cloud/setup.sh` and replay | Exit 0 | [Setup](prototype/cloud-setup.log) |
| Fresh pinned SDK download | Exit 0; all five file hashes verified | [SDK](prototype/sdk-clean-fetch.log) |
| `. tools/cloud/activate.sh` then `sh tests/run_jam_tests.sh` | Exit 0; 435 palette checks, 42 UI checks, native/WASM recording and audio contracts | [Prototype tests](prototype/jam-tests.log) |
| Activated `node tests/jam_preview_browser.mjs` | Exit 0; actual Chromium AudioWorklet and desktop/mobile interactions | [Browser tests](prototype/jam-browser.log) |
| Prototype C composition, host syntax only | Exit 0; empty diagnostic log | [Syntax](prototype/prototype-host-syntax.log) |
| Prototype with flash enabled | Expected compile-time rejection | [Storage guard](prototype/prototype-storage-guard.log) |
| Device prerequisites | Expected exit 1: vendor compiler absent; SDK valid | [Preflight](prototype/device-preflight.log) |

[Tool versions](toolchains/versions.txt), [Debian archive hashes](toolchains/SHA256SUMS) and signed [InRelease metadata](toolchains/debian-InRelease) retain tool provenance. The current setup log also records Python asset and Chromium versions. Playwright Core is pinned by `tools/cloud/browser/package-lock.json`.

[Source hashes](prototype/source-sha256.json) identify the firmware, browser, tools and tests used for this run. [Artifact hashes](prototype/artifact-sha256.json) identify generated browser files, the native audition WAV and the captured screens. Generated outputs live under ignored `build/` and can be reproduced using the commands above; these hashes identify this run without claiming cross-environment determinism.

The native audition is generated at `build/jam-tests/dusty-demo.wav`. It adds a demo-only Cm7/Abmaj7 phrase, bass and lead to the real palette; a new session itself starts with empty melodic patterns. Screenshots show the [FM-1 Home harness](screens/fm1-home.png), [browser desktop](screens/browser-desktop.png) and [browser mobile](screens/browser-mobile.png).

These checks do not establish FM-1 target size, CPU headroom, sound quality by listening, hardware input behavior or flashing compatibility. No target binary was built. See [current build status](../BUILD_STATUS.md) for the remaining work and the inherited sanitizer findings.
