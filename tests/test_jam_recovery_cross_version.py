#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Cold-restore synthetic manual saves in both directions across drum trials.

Uses actual old/current runtime adapters and their existing full-session
assertions. No device, user backup, shared generated-header writes or Git
checkout/worktree mutations are involved. Outputs stay in ignored build/.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "2ef4338d611b61d49efc4d2249183df976eddc83"
IDENTITY = "firmware/experimental/recovery/jr_asset_identity.h"
ASSETS = ["felucca_samples.h", "felucca_drumkits.h", "felucca_fm6.h", "felucca_tables.h"]
UNCHANGED = ["firmware/src/core.h", "firmware/src/jam_recovery_runtime.c",
             "firmware/src/jam_recovery_api.h", "firmware/src/jam_recovery_sidecars.h",
             "firmware/src/project.c", "firmware/src/storage.c",
             "firmware/src/jam_palette.c", "firmware/src/drum_synth.c",
             "firmware/src/engines.c", "tests/jam_recovery_runtime_test.c",
             "tests/jam_storage_harness.c", "tests/hostsim.c"]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(command: list[str], log: Path | None = None) -> str:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    output = result.stdout + result.stderr
    if log:
        log.write_text(output, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {shlex.join(command)}\n{output}")
    return output


def export_baseline(out: Path, revision: str) -> Path:
    # A fresh destination and regular-file-only extraction avoid following
    # archive links or replacing anything in the active checkout.
    destination = Path(tempfile.mkdtemp(prefix="baseline-", dir=out))
    archive = subprocess.check_output(
        ["git", "archive", "--format=tar", revision, "firmware", "tests"], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as source:
        for member in source:
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts or name.parts[0] not in {"firmware", "tests"}:
                raise ValueError(f"Unexpected archive path: {member.name}")
            target = destination.joinpath(*name.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as stream, target.open("xb") as output:
                    output.write(stream.read())
            else:
                raise ValueError(f"Archive entry must be a regular file: {member.name}")
    return destination


def preconditions(old: Path, gen: Path) -> dict:
    abi_files = sorted(set(UNCHANGED + [IDENTITY] +
        [p.relative_to(old).as_posix() for p in (old / "firmware/experimental/recovery").glob("*.[ch]")] +
        [p.relative_to(old).as_posix() for p in (old / "firmware/src").glob("eng_*.c")]))
    for name in abi_files:
        if (old / name).read_bytes() != (ROOT / name).read_bytes():
            raise ValueError(f"Cross-version ABI/core precondition changed: {name}")
    identity = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", (ROOT / IDENTITY).read_text()))
    if len(identity) != 64:
        raise ValueError("Canonical recovery identity must have exactly 64 bytes")
    digest = hashlib.sha256()
    for name in sorted(ASSETS):
        label, data = ("build/gen/" + name).encode(), (gen / name).read_bytes()
        digest.update(len(label).to_bytes(4, "little") + label + len(data).to_bytes(8, "little") + data)
    if digest.digest() != identity[:32]:
        raise ValueError("Generated assets differ from the unchanged recovery asset identity")
    return {"unchanged_files": {name: sha((ROOT / name).read_bytes()) for name in abi_files},
            "identity_hex": identity.hex(), "generated_assets_sha256": digest.hexdigest()}


def compile_fixture(source: Path, gen: Path, out: Path, name: str, compiler: list[str]) -> Path:
    fixture = (source / "tests/jam_recovery_runtime_test.c").read_text()
    entry = "int main(int argc, char **argv)"
    if fixture.count(entry) != 1:
        raise ValueError("Expected the existing runtime fixture's single main entry")
    # hostsim.c undefines main inside the include chain, so a preprocessor
    # main rename around an include would not survive. Rename only this build
    # copy's entry declaration; all existing runtime assertion bodies stay exact.
    wrapper = out / f"{name}.c"
    wrapper.write_text(fixture.replace(entry, "int runtime_fixture_main(int argc, char **argv)") + r'''
int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--emit")) {
        memset(nor, 255, sizeof nor); jrt_boot(); jrt_seed_session(1);
        check(jam_recovery_save(1) == 0, "cross-version synthetic session saves through actual adapter");
        if (failures) return 1;
        FILE *file = fopen(argv[2], "wb");
        if (!file) return 2;
        int written = fwrite(nor, 1, sizeof nor, file) == sizeof nor;
        int closed = fclose(file) == 0;
        return written && closed ? 0 : 2;
    }
    return runtime_fixture_main(argc, argv);
}
''', encoding="utf-8")
    binary = out / name
    run(compiler + ["-std=c11", "-O2", "-fwrapv", "-fno-strict-aliasing", "-Wno-int-to-pointer-cast",
                   "-iquote", str(source / "tests"), "-I" + str(gen),
                   "-I" + str(source / "firmware/src"), "-I" + str(source / "firmware/hal"),
                   str(wrapper), "-o", str(binary), "-lm"], out / f"{name}-compile.log")
    return binary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", default=BASELINE)
    parser.add_argument("--gen-dir", type=Path, default=ROOT / "build/gen")
    args = parser.parse_args()
    gen = args.gen_dir.resolve()
    out = ROOT / "build/jam-recovery-cross-version"
    out.mkdir(parents=True, exist_ok=True)
    revision = run(["git", "rev-parse", "--verify", args.baseline + "^{commit}"]).strip()
    old = export_baseline(out, revision)
    compatibility = preconditions(old, gen)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    baseline = compile_fixture(old, gen, out, "baseline-runtime", compiler)
    current = compile_fixture(ROOT, gen, out, "current-runtime", compiler)
    records = []
    images = []
    for writer, reader, direction in [(baseline, current, "baseline-to-current"),
                                      (current, baseline, "current-to-baseline")]:
        image = out / f"{direction}-synthetic-nor.bin"
        emitted = run([str(writer), "--emit", str(image)], out / f"{direction}-emit.log")
        restored = run([str(reader), "--cold", str(image)], out / f"{direction}-restore.log")
        checks = len(re.findall(r"^storage: .* ok$", restored, re.MULTILINE))
        if checks != 17 or "FAIL" in restored or "FAIL" in emitted:
            raise ValueError(f"{direction} did not pass all 17 existing cold-restore checks")
        data = image.read_bytes()
        if len(data) != 0x100000:
            raise ValueError("Synthetic fixture must contain the complete 1 MiB NOR image")
        images.append(data)
        records.append({"direction": direction, "cold_restore_checks": checks,
                        "emit_checks": len(re.findall(r"^storage: .* ok$", emitted, re.MULTILINE)),
                        "nor_bytes": len(data), "nor_sha256": sha(data), "ok": True})
    if images[0] != images[1]:
        raise ValueError("Old/current real adapters emitted different synthetic NOR bytes")
    if preconditions(old, gen) != compatibility:
        raise ValueError("Compatibility inputs changed during the check")
    summary = {"schema_version": 1, "ok": True, "baseline_revision": revision,
               "current_checkout_revision": run(["git", "rev-parse", "HEAD"]).strip(),
               "current_checkout_may_include_uncommitted_changes": bool(
                   run(["git", "status", "--porcelain", "--untracked-files=no"]).strip()),
               "compatibility": compatibility, "directions": records,
               "actual_nor_bytes_identical": True, "total_cold_restore_checks": 34,
               "physical_device_accessed": False, "user_backup_used": False}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print("Cross-version manual save: 34 cold-restore checks passed; old/new 1 MiB NOR images identical.")
    print(f"Evidence: {out / 'summary.json'}")


if __name__ == "__main__":
    main()
