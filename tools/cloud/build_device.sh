#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Explicit early prototype target; no storage/OTA until the JAM1 milestone.
set -euo pipefail
cd "$(dirname "$0")/../.."
. tools/cloud/activate.sh
python3 tools/cloud/check_device.py
export JAMBOX_PROTO=1
export FELUCCA_FLASH=0
export FELUCCA_OTA=0
export FELUCCA_ICONS=0
exec ./build.sh "$@"
