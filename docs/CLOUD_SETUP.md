# JamBox development environment

The browser, native C and FM-1 target build workflows are available in this Debian 13 amd64 cloud environment. The owner supplied the official-download compiler archive after its redirect host was blocked. Sloop and JamBox now compile for pi32v2; [baseline device evidence](build-evidence/device-build/README.md) and [update-isolation evidence](build-evidence/update-isolation/README.md) distinguish software checks from the remaining hardware work.

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
- `/workspace/toolchains/jieli`: the pinned 20260730.1 device compiler archive, extraction, acquisition lock and `toolchain` symlink.

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

With the retained compiler cache, verify/reuse the pinned archive and build:

```sh
bash tools/cloud/setup.sh --device
bash tools/cloud/build_device.sh
python3 tools/cloud/report_device.py > build/device-memory.json
```

On a fresh machine, `setup.sh --device` tries verified HTTPS at `https://pkgman.jieliapp.com/s/linux-toolchain` and requires SHA-256 `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`. On October 13 the endpoint redirected to `jl-update.oss-cn-shenzhen.aliyuncs.com`; that host returned proxy CONNECT 403. Both hostnames are saved in the environment network draft, but a saved setting does not prove runtime access. If still blocked, import the owner's downloaded file, then repeat the commands above:

```sh
python3 tools/cloud/prepare_toolchain.py \
  --archive /path/to/jieli-linux-toolchains-20260730.1.tar.xz \
  --expected-sha256 a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd
```

The archive is 26,027,508 bytes and reports Clang 4.0.1. `prepare_toolchain.py` records the acquisition method, SHA-256 and version outside the repository. This instance's lock explicitly records an **owner-provided archive**; the agent did not observe that download's HTTPS transfer. The digest pins those bytes, not a vendor signature. Mismatches are errors; do not replace the pin simply because the moving vendor URL changes.

`build_device.sh` performs the SDK/compiler preflight, then sets `JAMBOX_PROTO=1`, `FELUCCA_PERSIST=0`, `FELUCCA_FLASH=0`, `FELUCCA_OTA=0`, `FELUCCA_OTA_DRYRUN=0`, `FELUCCA_OTA_RAMONLY=0`, `FELUCCA_ICONS=0` and `FELUCCA_SLICE=0`. It invokes `bash build.sh` because the inherited script is not executable in Git. This default remains the diagnostic target.

Milestone 2's update-enabled software target and isolation tests use:

```sh
bash tools/cloud/build_device.sh --with-updater
python3 tools/cloud/report_device.py > build/device-memory.json
sh tests/jam_storage_run.sh build/felucca.fwsc
sh tests/run_jam_emu_tests.sh
```

Put `--with-updater` first. It changes `FELUCCA_FLASH` and `FELUCCA_OTA` to `1`; app persistence stays `0`. The policy rejects JamBox builds with persistence enabled or an updater configured for dry-run/RAM-only resume. Normal Sloop builds continue to default persistence to their flash capability. Both targets reuse `build/felucca.*`, so preserve evidence before switching targets.

The update-enabled target starts a fresh Dusty RAM session, accepts only strictly validated read-only panel calibration, and permits bounded update writes while rejecting app-data saves/restores. The software test runner uses synthetic NOR and transport fixtures; it never connects to an FM-1. Its final command list, outcomes and current target memory report are retained in [update-isolation evidence](build-evidence/update-isolation/README.md). **Neither generated `.fwsc` is ready for owner installation.** Identity/splash/manifest work, installer transition simulation, read-only inventory and the physical recovery gates remain in [the device plan](FM1_DEVICE_PLAN.md). No installer or firmware release is published by these commands.

The memory reporter checks the app and loader regions, reserves, unresolved/runtime-helper symbols and compressed loader headers/size. It cannot measure physical stack use or audio deadlines. Build outputs stay under ignored `build/`; the public repository retains text evidence and hashes, not a firmware release or the proprietary compiler archive.

Use the existing checkout: cloud tasks are already isolated, so do not create a Git worktree unless the user explicitly requests one. REA remains available through `npm_config_cache=/workspace/.npm-cache npx --yes rea-agents@6.4.0 <command>`; provider/MCP registration should be checked after a new task starts.
