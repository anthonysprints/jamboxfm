#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build-time inputs and provenance shared by the target builder and packager.

This receipt binds a local build to its inputs; it is not a digital signature.
It never authorizes installation or downloads a compiler.
"""
import ast
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess

from cloud.prepare_sdk import FILE_HASHES as SDK_FILES
from cloud.prepare_toolchain import prepare as verify_toolchain, tree_inventory

VERSION = "0.1.0-dev"
IDENTITIES = {"app": "FM-1_700", "loader": "ota-FM-1_700", "package": "FM-1_700", "rescue": "FM-1_000"}
COMPILER_ARCHIVE_SHA256 = "a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd"
FLAGS = ("JAMBOX_PROTO", "FELUCCA_FLASH", "FELUCCA_PERSIST", "FELUCCA_OTA", "FELUCCA_OTA_DRYRUN",
         "FELUCCA_OTA_RAMONLY", "FELUCCA_ICONS", "FELUCCA_SLICE", "FELUCCA_CDC", "FELUCCA_UART",
         "JAMBOX_RECOVERY")
REQUIRED_FLAGS = {"JAMBOX_PROTO": 1, "FELUCCA_FLASH": 1, "FELUCCA_PERSIST": 0, "FELUCCA_OTA": 1,
                  "FELUCCA_OTA_DRYRUN": 0, "FELUCCA_OTA_RAMONLY": 0, "FELUCCA_ICONS": 0, "FELUCCA_SLICE": 0,
                  "JAMBOX_RECOVERY": 1}
GENERATOR_INPUTS = (
    "build.sh", "tools/build.py", "tools/device_build_receipt.py", "tools/fm1pkg_make.py", "tools/lz4blk.py",
    "tools/gen_font.py", "tools/gen_icons.py", "tools/gen_tables.py", "tools/gen_samples.py", "tools/sampleio.py",
    "tools/gen_waves.py", "tools/gen_drumkits.py", "tools/drumkit_levels.json", "tools/gen_fm6_patches.py",
    "tools/cloud/prepare_sdk.py", "tools/cloud/prepare_toolchain.py", "tools/cloud/build_device.sh",
    "firmware/app.ld", "firmware/loader/loader.ld", "assets/icons.json", "assets/fonts/ter-u16b.bdf",
    "assets/fonts/Terminus-LICENSE.txt",
)
GENERATED = tuple("build/gen/" + name for name in (
    "felucca_font.h", "felucca_icons.h", "felucca_tables.h", "felucca_samples.h",
    "felucca_drumkits.h", "felucca_fm6.h",
))
ARTIFACTS = ("felucca.bin", "felucca.elf", "loader/loader.bin", "loader/loader.elf", "loader/ota.bin")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def jam_identity(root):
    text = (root / "firmware/src/jam_identity.h").read_text()
    values = {}
    for name in ("JAMBOX_NAME", "JAMBOX_RELEASE", "JAMBOX_APP_ID", "JAMBOX_LOADER_ID", "JAMBOX_RESCUE_ID"):
        match = re.search(r'^#define\s+' + name + r'\s+("[^"\n]+")\s*$', text, re.M)
        require(match is not None, f"Missing canonical identity {name}")
        values[name] = ast.literal_eval(match[1])
    require(values["JAMBOX_RELEASE"] == VERSION and values["JAMBOX_APP_ID"] == IDENTITIES["app"] and
            values["JAMBOX_LOADER_ID"] == IDENTITIES["loader"] and values["JAMBOX_RESCUE_ID"] == IDENTITIES["rescue"],
            "Firmware identity differs from candidate policy")
    return values


def file_sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def hashes(root, names):
    result = {}
    for name in sorted(set(names)):
        path = root / name
        require(not path.is_symlink() and path.is_file() and path.resolve().is_relative_to(root.resolve()),
                f"Missing or unsafe build input: {name}")
        result[name] = file_sha(path)
    return result


def input_pool(root):
    """Snapshot possible C dependencies before compilation; receipt retains actual ones."""
    names = set(GENERATOR_INPUTS)
    names.update(path.relative_to(root).as_posix() for path in (root / "firmware").rglob("*")
                 if path.suffix in (".c", ".h", ".S", ".ld"))
    names.update(path.relative_to(root).as_posix() for path in (root / "assets/samples-cc0").rglob("*")
                 if path.is_file())
    return hashes(root, names)


def generator_inputs(root):
    return set(GENERATOR_INPUTS) | {path.relative_to(root).as_posix()
        for path in (root / "assets/samples-cc0").rglob("*") if path.is_file()}


def dependency_inputs(root, build):
    names = set()
    for name in ("app.d", "crt0.d", "fm1_vec.d", "fm1_isr.d", "loader/loader.d", "loader/crt0_ldr.d"):
        data = (build / name).read_text().replace("\\\n", " ")
        require(":" in data, f"Invalid compiler dependency file: {name}")
        for entry in shlex.split(data.split(":", 1)[1]):
            path = Path(entry)
            path = (root / path).resolve() if not path.is_absolute() else path.resolve()
            require(path.is_relative_to(root.resolve()), f"Unexpected external compiler dependency: {entry}")
            names.add(path.relative_to(root.resolve()).as_posix())
    return names


def configuration(root, build, unit, flags, tc):
    """Ask the actual target preprocessor for final Boolean macros and identity."""
    source = "../firmware/src/felucca.c" if unit == "app" else "../firmware/loader/loader.c"
    probe = build / (unit + "-configuration.c")
    lines = [f'#include "{source}"']
    for name in FLAGS:
        lines += [f"#if defined({name}) && ({name})", f"JAMBOX_RECEIPT_FLAG_{name} 1",
                  "#else", f"JAMBOX_RECEIPT_FLAG_{name} 0", "#endif"]
    lines += ["JAMBOX_RECEIPT_ID FELUCCA_ID"]
    if unit == "app":
        lines += ["JAMBOX_RECEIPT_VERSION FELUCCA_VERSION"]
    probe.write_text("\n".join(lines) + "\n")
    output = tc("cc", *flags, "-E", "-P", probe)
    actual = {match[1]: int(match[2]) for match in re.finditer(
        r"^JAMBOX_RECEIPT_FLAG_(\w+) ([01])$", output, re.M)}
    require(set(actual) == set(FLAGS), f"Incomplete {unit} compiler configuration")
    result = {"flags": actual}
    for label, field in (("ID", "identity"), ("VERSION", "version")):
        match = re.search(r"^JAMBOX_RECEIPT_" + label + r" (.+)$", output, re.M)
        if match:
            value = ast.literal_eval(match[1])
            require(isinstance(value, str), f"Invalid compiled {field}")
            result[field] = value
    return result


def source_state(root, names):
    top = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=root, capture_output=True, text=True)
    run = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True)
    if run.returncode or top.returncode or Path(top.stdout.strip()).resolve() != root.resolve():
        return {"revision": None, "dirty": None, "provenance": "unversioned-source-export"}
    tracked = subprocess.check_output(["git", "ls-files", "-z", "--", *sorted(names)], cwd=root).decode().split("\0")
    diff = subprocess.run(["git", "diff", "--quiet", "HEAD", "--", *sorted(names)], cwd=root)
    return {"revision": run.stdout.strip(), "dirty": bool(diff.returncode or set(names) - set(tracked)),
            "provenance": "git-checkout-with-input-hashes"}


def external_inputs(toolchain, sdk, strict):
    """Verify the installed owner-pinned compiler without allowing a network acquisition."""
    toolchain = toolchain.resolve()
    base = toolchain.parent
    lock = base / "toolchain-lock.json"
    archive = base / "jieli-linux-toolchain.tar.xz"
    if strict:
        require(lock.is_file() and archive.is_file(), "JamBox requires the pinned compiler archive and acquisition lock")
        verify_toolchain(base, expected_sha256=COMPILER_ARCHIVE_SHA256)
        require((base / "toolchain").resolve() == toolchain, "Build compiler differs from verified compiler")
    compiler = json.loads(lock.read_text()) if lock.is_file() else {"acquisition_trust": "No acquisition lock supplied"}
    # Absolute installation paths do not affect corresponding-source provenance.
    compiler.pop("JIELI_TOOLCHAIN", None)
    if "compiler_version" in compiler:
        compiler["compiler_version"] = "\n".join(line for line in compiler["compiler_version"].splitlines()
                                                   if not line.startswith("InstalledDir:"))
    compiler["executables_sha256"] = {name: file_sha(toolchain / name) for name in
        ("pi32v2/bin/clang", "pi32v2/bin/ld", "common/bin/objcopy", "common/bin/objdump")}
    compiler["installed_tree_sha256"] = sha(json_bytes(tree_inventory(toolchain)))
    if archive.is_file():
        compiler["cached_archive_sha256"] = file_sha(archive)
    actual_sdk = hashes(sdk, SDK_FILES)
    if strict:
        require(actual_sdk == SDK_FILES, "SDK files differ from the pinned licensed subset")
    sdk_receipt = sdk / "JAMBOX-SDK-FILES.json"
    return {"compiler": compiler, "sdk": {"files_sha256": actual_sdk,
            "provenance": json.loads(sdk_receipt.read_text()) if sdk_receipt.is_file() else None}}


def finish(root, build, before, external, app_configuration, loader_configuration, product, package_name, commands):
    require(input_pool(root) == before, "Build input set or contents changed during compilation; rebuild")
    dependencies = dependency_inputs(root, build)
    generated = set(GENERATED) & dependencies
    inputs = (dependencies - set(GENERATED)) | generator_inputs(root)
    require(all(name in before for name in inputs), "Compiler consumed an unrecorded source input")
    after = hashes(root, inputs)
    require(after == {name: before[name] for name in sorted(inputs)}, "Build inputs changed during compilation; rebuild")
    receipt = {"schema_version": 1, "kind": "jambox-device-build", "source": source_state(root, inputs),
        "inputs_sha256": after, "generated_inputs_sha256": hashes(root, generated),
        "configuration": {"app": app_configuration, "loader": loader_configuration, "package_identity": product},
        "toolchain": external["compiler"], "sdk": external["sdk"],
        "python_version": platform.python_version(),
        "commands": sorted(commands, key=lambda command: json.dumps(command, sort_keys=True)),
        "artifacts": {name: {"sha256": file_sha(build / name), "size_bytes": (build / name).stat().st_size}
                      for name in (*ARTIFACTS, package_name)}}
    temporary = build / "device-build-receipt.json.partial"
    temporary.write_bytes(json_bytes(receipt))
    temporary.replace(build / "device-build-receipt.json")
    return receipt
