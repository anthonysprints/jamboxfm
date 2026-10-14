#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Local, private factory fixture; never copied into the public source or site.
set -eu
cd "$(dirname "$0")/.."
factory=${1:-/workspace/deps/jambox-return/FM-1-V15.fwsc}
out=${JAM_FACTORY_OTA_OUT:-build/jam-factory-ota}
python3 - "$factory" <<'PY'
import hashlib
from pathlib import Path
import sys

path = Path(sys.argv[1])
if not path.is_file():
    raise SystemExit(f"Factory V15 fixture missing: {path}")
data = path.read_bytes()
expected = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"
if len(data) != 699956 or hashlib.sha256(data).hexdigest() != expected:
    raise SystemExit("Refusing fixture: expected the exact 699956-byte owner-supplied factory V15 package")
print(f"Factory V15 fixture: {len(data)} bytes; SHA-256 {expected}")
PY
mkdir -p "$out"
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -o "$out/jam_factory_ota_test" tests/jam_factory_ota_test.c
"$out/jam_factory_ota_test" "$factory"
