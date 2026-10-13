# FM-1-style browser emulator results

October 13, 2026. The complete saved command passed with exit 0:

```sh
. tools/cloud/activate.sh
sh tests/run_jam_emu_tests.sh
```

[Full log](full-tests.log): package rebuild, 27 native control/UI/audio checks, and real Chromium desktop/mobile/standalone checks. Browser tests cover physical knobs, real LCD changes, held layers and deep pages, chord recording, pointer and keyboard release, panic, pause during input, rapid PLAY presses and canceled master changes. They start and close their own server.

The single HTML played audio with every further HTTP request blocked after its initial document. Its actual source-download link produced bytes identical to `build/jambox-source.zip`. Managed Chromium rejects local `file://` navigation; these results do not claim a tested double-click launch. Public HTTPS hosting is not configured or verified.

[Clean-source rebuild](source-rebuild.log): all 277 source files matched the archive manifest and checkout; the extracted source reproduced the exact 679,279-byte WASM and all four frontend files. [Packaging log](package.log) and [artifact/source hashes](sha256.json) identify the deliverables. The static publication tree contains no inherited installer or `.fwsc`.

[Independent worklet probe](worklet-review.log) confirms audible same-batch key taps, ordered double PLAY and relative master changes. [Layer probe](layer-review.log) confirms the SEL action survives either modifier/key release order. These probes are supplementary; they are not hardware audio-timing measurements.

Screenshots: [desktop](../screens/emu-desktop.png), [mobile](../screens/emu-mobile.png), [self-contained page](../screens/emu-standalone.png). The emulator runs a temporary session and does not access or flash an attached FM-1. See [opening, building and hosting instructions](../../WEB_EMULATOR.md).
