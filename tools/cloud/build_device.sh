#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Software bring-up targets only; none is an owner-installation release.
set -euo pipefail
cd "$(dirname "$0")/../.."
rm -f build/device-build-receipt.json build/device-build-receipt.json.partial
. tools/cloud/activate.sh
python3 tools/cloud/check_device.py
export JAMBOX_PROTO=1
export JAMBOX_RECOVERY=0
export FELUCCA_PERSIST=0
export FELUCCA_FLASH=0
export FELUCCA_OTA=0
export FELUCCA_OTA_DRYRUN=0
export FELUCCA_OTA_RAMONLY=0
export FELUCCA_ICONS=0
export FELUCCA_SLICE=0
case ${1:-} in
    --with-updater)
        export FELUCCA_FLASH=1 FELUCCA_OTA=1 JAMBOX_RECOVERY=1
        shift
        ;;
    --with-recovery)
        # Dedicated development target: leave every inherited object writer
        # disabled. Recovery owns only its explicit fourteen-sector allocation.
        export FELUCCA_FLASH=1 FELUCCA_OTA=1 JAMBOX_RECOVERY=1
        shift
        ;;
esac
exec bash build.sh "$@"
