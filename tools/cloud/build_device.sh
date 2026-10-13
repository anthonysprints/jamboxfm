#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Diagnostic compile only: app persistence and update/rescue support are off.
set -euo pipefail
cd "$(dirname "$0")/../.."
. tools/cloud/activate.sh
python3 tools/cloud/check_device.py
export JAMBOX_PROTO=1
export FELUCCA_FLASH=0
export FELUCCA_OTA=0
export FELUCCA_OTA_DRYRUN=0
export FELUCCA_ICONS=0
export FELUCCA_SLICE=0
exec bash build.sh "$@"
