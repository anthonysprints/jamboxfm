#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Software bring-up targets only; neither is an owner-installation release.
set -euo pipefail
cd "$(dirname "$0")/../.."
rm -f build/device-build-receipt.json build/device-build-receipt.json.partial
. tools/cloud/activate.sh
python3 tools/cloud/check_device.py
export JAMBOX_PROTO=1
export FELUCCA_PERSIST=0
export FELUCCA_FLASH=0
export FELUCCA_OTA=0
export FELUCCA_OTA_DRYRUN=0
export FELUCCA_OTA_RAMONLY=0
export FELUCCA_ICONS=0
export FELUCCA_SLICE=0
if [[ ${1:-} == --with-updater ]]; then
    export FELUCCA_FLASH=1 FELUCCA_OTA=1
    shift
fi
exec bash build.sh "$@"
