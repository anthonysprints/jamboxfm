# JamBox development environment

The browser and native C workflow is available in this Debian 13 amd64 cloud environment. The FM-1 target compiler is still blocked by the network policy at `pkgman.jieliapp.com`; its required network addition has been saved to the environment configuration draft.

## Install and activate

From the existing checkout:

```sh
cd /workspace/jamboxfm
bash tools/cloud/setup.sh
. tools/cloud/activate.sh
```

Setup keeps dependencies outside the checkout:

- `/workspace/deps/jambox-python`: Pillow 12.3.0 and fontTools 4.61.1.
- `/workspace/toolchains/debian-browser`: Emscripten 3.1.69, Clang/LLVM 19.1.7 and Binaryen 120, extracted from pinned Debian packages.
- `/workspace/deps/ac79-sdk`: five pinned files, including all three required SDK binaries, their license and provenance.
- `/workspace/deps/jambox-browser`: Playwright Core 1.58.2 for the browser test, using the cloud image's Chromium.

The browser package hashes were checked against Debian's signed package metadata. The reusable installer verifies every archive against those recorded hashes and retains HTTPS certificate checking. It does not install system packages. Node, Python, Debian runtime libraries and standard C development headers are prerequisites supplied by this cloud image. Package pins are in [packages.json](../tools/cloud/packages.json).

Setup and replay passed in this instance. SDK acquisition also passed into a fresh destination using the pinned GitHub files. Subsequent tasks must source `tools/cloud/activate.sh` before builds; shell activation and running servers do not survive automatically between tasks.

## Browser and native work

Build, package and test the device-style emulator with:

```sh
sh tests/run_jam_emu_tests.sh
```

This runs the native physical-control checks and actual Chromium tests, including the single-file package with further network requests blocked. The development output is `build/jam-emu/`; the standalone HTML and isolated publication root are described in [WEB_EMULATOR.md](WEB_EMULATOR.md). To build/package without running tests, use `python3 tools/package_jam_emu.py`. No target compiler or attached device is needed.

The emulator can be served internally using the server command below with `--directory build/jam-emu`. The cloud interface does not turn that server into a public preview URL. A public site requires publishing the isolated `build/jambox-web/` directory to an HTTPS static host; the inherited `docs/` contains Sloop's installer and is not the JamBox publish root.

Build and test the earlier four-part workbench with:

```sh
sh tests/run_jam_tests.sh
```

The output is `build/jam-preview/`. Serve it for internal development with:

```sh
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/jam-preview
```

Verify the server with a local HTTP request and the browser smoke test. Audio starts only after the page's Start audio action. The page uses the compiled C instrument locally; no FM-1 connection is required. The cloud onboarding interface does not expose a user-facing localhost preview link.

The browser smoke test starts and closes its own temporary server and exercises Chromium's actual AudioWorklet, keyboard, pointer and mobile layout:

```sh
node tests/jam_preview_browser.mjs
```

Activation sets `PLAYWRIGHT_MODULE` to the pinned external dependency and defaults `CHROMIUM_PATH` to `/usr/bin/chromium`. The browser dependency lockfile is in `tools/cloud/browser/`. A Chromium installation is a prerequisite supplied by this cloud image; setup checks its version.

The inherited Sloop preview remains independently buildable:

```sh
python3 tools/build_preview.py
sh tests/run_preview_tests.sh
```

The unchanged pinned upstream source passed this native/WASM comparison for all twelve engines and the existing sequencing/editor checks. Logs are retained under [build-evidence/baseline/](build-evidence/baseline/).

## Device compiler and prototype build

Once runtime access to the vendor endpoint is available:

```sh
bash tools/cloud/setup.sh --device
bash tools/cloud/build_device.sh
```

`prepare_toolchain.py` records the acquired compiler archive's SHA-256 and version under `/workspace/toolchains/jieli/`. The first hash records a TLS-verified official download; it is not represented as a vendor signature. Later runs reject differing bytes at the moving vendor URL.

`build_device.sh` performs the SDK/compiler preflight, then sets `JAMBOX_PROTO=1`, `FELUCCA_FLASH=0`, `FELUCCA_OTA=0` and `FELUCCA_ICONS=0`. The prototype uses a fresh RAM session. Persistent projects, migration and the JamBox installer remain separate milestones. The C entry point rejects a prototype build with flash storage or OTA enabled.

The preflight currently reports the absent compiler. No FM-1 target binary, target memory map or hardware update has been validated. The full inherited test runner also requires a built `.fwsc`; targeted host tests do not count as that full suite.

Use the existing checkout: cloud tasks are already isolated, so do not create a Git worktree unless the user explicitly requests one. REA remains available through `npm_config_cache=/workspace/.npm-cache npx --yes rea-agents@6.4.0 <command>`; provider/MCP registration should be checked after a new task starts.
