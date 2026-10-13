#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Native policy/real-unit tests only; no device or target compiler is invoked.
set -eu
cd "$(dirname "$0")/.."
out=build/jam-tests
gen=${JAM_GEN_DIR:-build/gen}
package=${1:-build/felucca.fwsc}
if [ ! -f "$package" ]; then
    echo "OTA fixture missing: $package. Build the device package first, or pass an existing .fwsc." >&2
    exit 2
fi
mkdir -p "$out" "$gen"
for entry in tables samples fm6_patches drumkits; do
    case "$entry" in
        tables) header=felucca_tables.h ;;
        samples) header=felucca_samples.h ;;
        fm6_patches) header=felucca_fm6.h ;;
        drumkits) header=felucca_drumkits.h ;;
    esac
    if [ ! -f "$gen/$header" ] || [ "tools/gen_$entry.py" -nt "$gen/$header" ]; then
        python3 "tools/gen_$entry.py" "$gen/$header"
    fi
done
for unit in harness ota sloop; do
    "${CC:-cc}" -O2 -fwrapv -fno-strict-aliasing -Wno-int-to-pointer-cast \
        -I"$gen" -Ifirmware/src -Ifirmware/hal \
        -o "$out/jam_storage_$unit" "tests/jam_storage_$unit.c" -lm
done
"$out/jam_storage_harness"
"$out/jam_storage_sloop"
"$out/jam_storage_ota" "$package"

# Firmware preprocessing exercises the actual composition guards, including
# OTA-without-FLASH (not duplicated in a test-only policy model).
"${CC:-cc}" -E -DJAMBOX_PROTO=1 -DFELUCCA_FLASH=1 -DFELUCCA_OTA=1 \
    -DFELUCCA_ICONS=0 -DFELUCCA_SLICE=0 -I"$gen" -Ifirmware/src -Ifirmware/hal \
    firmware/src/felucca.c >/dev/null
reject_config() {
    tag=$1
    diagnostic=$2
    shift 2
    if "${CC:-cc}" -E -DJAMBOX_PROTO=1 -DFELUCCA_ICONS=0 -DFELUCCA_SLICE=0 \
        -I"$gen" -Ifirmware/src -Ifirmware/hal "$@" firmware/src/felucca.c \
        >/dev/null 2>"$out/jam_storage_guard_$tag.log"; then
        echo "Invalid firmware configuration unexpectedly accepted: $tag" >&2
        exit 1
    fi
    if ! grep -F "$diagnostic" "$out/jam_storage_guard_$tag.log" >/dev/null; then
        cat "$out/jam_storage_guard_$tag.log" >&2
        echo "Configuration failed for a reason other than the intended guard: $tag" >&2
        exit 1
    fi
    echo "Configuration guard $tag: passed"
}
reject_config persist 'JamBox temporary sessions require FELUCCA_PERSIST=0' \
    -DFELUCCA_FLASH=1 -DFELUCCA_OTA=1 -DFELUCCA_PERSIST=1
reject_config noflash 'FELUCCA_OTA requires FELUCCA_FLASH' \
    -DFELUCCA_FLASH=0 -DFELUCCA_OTA=1 -DFELUCCA_PERSIST=0
reject_config dryrun 'JamBox updater requires real writes and a persistent resume record' \
    -DFELUCCA_FLASH=1 -DFELUCCA_OTA=1 -DFELUCCA_OTA_DRYRUN=1
reject_config ramonly 'JamBox updater requires real writes and a persistent resume record' \
    -DFELUCCA_FLASH=1 -DFELUCCA_OTA=1 -DFELUCCA_OTA_RAMONLY=1

node tests/jam_editor_storage_test.mjs
for unit in jam_update_transport_test recovery_test midi_uart_test; do
    "${CC:-cc}" -O2 -Wno-int-to-pointer-cast -o "$out/jam_storage_$unit" "tests/$unit.c"
    "$out/jam_storage_$unit"
done

# Same synthetic older-app fixture as the inherited loader regression suite.
# This packages existing binaries only; AC79_SDK must refer to the configured
# SDK used by the device build. No target compilation or device access occurs.
app=${JAM_APP_BIN:-build/felucca.bin}
ota=${JAM_OTA_BIN:-build/loader/ota.bin}
if [ ! -f "$app" ] || [ ! -f "$ota" ]; then
    echo 'Loader fixture requires built app and OTA binaries (JAM_APP_BIN/JAM_OTA_BIN can override).' >&2
    exit 2
fi
head -c 200000 "$app" > "$out/jam_storage_old_app.bin"
python3 tools/fm1pkg_make.py "$out/jam_storage_old_app.bin" "$ota" "$out/jam_storage_old.fwsc"
for proto in 0 1; do
    "${CC:-cc}" -O2 -DJAMBOX_PROTO="$proto" -o "$out/jam_storage_loader_$proto" tests/ldr_test.c
    "$out/jam_storage_loader_$proto" "$out/jam_storage_old.fwsc" "$package"
done
