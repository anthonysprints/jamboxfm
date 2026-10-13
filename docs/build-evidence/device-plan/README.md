# FM-1 device-plan research evidence

October 13, 2026. These are offline research checks, not a target build or hardware installation.

The [device plan](../../FM1_DEVICE_PLAN.md) and detailed [FM-1-RE](../../research/fm1-re-device.md) / [Dinghy](../../research/dinghy-device.md) audits distinguish source findings, upstream hardware reports and our own executed checks. Exact source commits are in [source-pins.json](source-pins.json).

## Executed checks

- [FM-1-RE offline tests](fm1-re-offline-tests.log): 27 passed. Hardware paths in the flash-state tests use mocks/FakeLink. An ALSA test loads a library and checks a structure layout, without opening a MIDI port. The captured reflash record is upstream evidence, not independently reproduced hardware behavior.
- [Packet comparison](fm1-re-jambox-response-comparison.log): 1,028 byte-identical replies between our Python installer and pinned FM-1-RE. Covers payload sizes 0–512 with two address/flag pairs, including the 481-byte final-block case, and both completion responses. Only packet-builder functions are called; no packet is transmitted.
- Current device prerequisite check: the three SDK binary hashes verified; the pi32v2 compiler/linker and vendor objcopy/objdump are missing. The official compiler endpoint returned proxy CONNECT 403 in the preceding readiness check. No target firmware build was attempted or claimed.

## Reproduce the offline checks

Use the exact source pins, not moving branch tips. Research checkouts are dependencies under scratch, not additional project worktrees. Inspect upstream test entrypoints before running them.

```sh
cd /workspace/scratch/fm1-device-plan/fm1-re
PYTHONDONTWRITEBYTECODE=1 FM1_OTA_LOG=/dev/null python3 -m unittest discover -s tools/tests -v
```

From the JamBox checkout, reproduce the pure packet comparison:

```sh
PYTHONDONTWRITEBYTECODE=1 FM1_OTA_LOG=/dev/null python3 - <<'PY'
import importlib.util
from pathlib import Path
import sys

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module

ours = load('jambox_installer', Path('tools/fm1_install.py'))
reference = load('fm1_re_ota', Path('/workspace/scratch/fm1-device-plan/fm1-re/tools/fm1_ota.py'))
count = 0
for address, flag in [(0x000AAB20, 0), (0xFFFFFFFF, 0xFF)]:
    for length in range(513):
        payload = bytes((i * 37 + 0x80) & 255 for i in range(length))
        assert ours.response(address, payload, flag) == reference.build_response(address, payload, length, flag)
        count += 1
for address in (0xE0000000, 0xF0000000):
    payload = b'success\0'
    assert ours.response(address, payload) == reference.build_response(address, payload, len(payload))
    count += 1
print(f'{count} byte-identical responses; no device I/O')
PY
```

The existing identity-parser validation gap, partial-SysEx transport behavior, target compilation, installer simulation and all hardware tests remain separate implementation/validation work. The above packet equality does not pass those gates.
