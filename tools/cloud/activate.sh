#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Source from any directory; dependencies live outside the source checkout.
if [ ! -f /workspace/deps/jambox-python/bin/activate ] || [ ! -f /workspace/toolchains/debian-browser/env.sh ]; then
    echo 'Run bash tools/cloud/setup.sh from the JamBox checkout first.' >&2
    return 1
fi
. /workspace/deps/jambox-python/bin/activate
. /workspace/toolchains/debian-browser/env.sh
export AC79_SDK=/workspace/deps/ac79-sdk
export JIELI_TOOLCHAIN=/workspace/toolchains/jieli/toolchain
export PLAYWRIGHT_MODULE=/workspace/deps/jambox-browser/node_modules/playwright-core/index.mjs
export CHROMIUM_PATH="${CHROMIUM_PATH:-/usr/bin/chromium}"
