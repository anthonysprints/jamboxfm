#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Debian cloud setup. --device additionally attempts the official device compiler.
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --device ) ]]; then
    echo 'Usage: bash tools/cloud/setup.sh [--device]' >&2
    exit 2
fi
if [[ ! -x /workspace/deps/jambox-python/bin/python ]]; then
    python3 -m venv /workspace/deps/jambox-python
fi
/workspace/deps/jambox-python/bin/python -m pip install --disable-pip-version-check --cache-dir /workspace/deps/pip-cache -r tools/cloud/python-requirements.txt
python3 tools/cloud/install_browser.py
python3 tools/cloud/prepare_sdk.py
mkdir -p /workspace/deps/jambox-browser
cp tools/cloud/browser/package.json tools/cloud/browser/package-lock.json /workspace/deps/jambox-browser/
npm_config_cache=/workspace/.npm-cache npm ci --prefix /workspace/deps/jambox-browser --ignore-scripts --no-audit --no-fund
. tools/cloud/activate.sh
python3 -c 'import PIL, fontTools; print("Python assets ready:", PIL.__version__, fontTools.__version__)'
emcc --version
"$CC" --version
"$CHROMIUM_PATH" --version
if [[ ${1:-} == --device ]]; then
    python3 tools/cloud/prepare_toolchain.py
    python3 tools/cloud/check_device.py
fi
