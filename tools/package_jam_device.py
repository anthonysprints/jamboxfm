#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and package a local JamBox FM-1 software candidate, source and evidence.

Nothing connects to a device, uploads files, or enables a physical install.
By default build the updater-enabled target first; --no-build verifies an
existing build and its receipt against the current source and pinned tools.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import device_build_receipt as receipt
import fm1pkg_make
import package_jam_emu as source_package
from cloud import report_device

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
OUTPUT = BUILD / "jambox-device"
FIRMWARE = f"jamboxfm-{receipt.VERSION}.fwsc"
SOURCE = "jamboxfm-source.zip"
SLOOP_26 = {"family": "sloop", "version": "2.6", "product": "FM-1_900", "size_bytes": 610019,
    "sha256": "199829bb5b892feae9058158dfed7a137c83ec4d8dfc1223761be1f5614ad6a2"}
TRANSITIONS = ["jambox-normal-to-jambox", "jambox-rescue-to-jambox", "jambox-loader-to-jambox",
               "sloop-2.6-to-jambox", "jambox-normal-to-sloop-2.6", "jambox-rescue-to-sloop-2.6"]
FORMAT_EVIDENCE = {name: True for name in ("package_header_crc", "entry_headers_crc", "payload_crc",
    "chip_key_match", "canonical_reconstruction", "app_loader_elf_match", "memory_regions")}
require = receipt.require


def descriptor(name, data):
    return {"file": name, "sha256": receipt.sha(data), "size_bytes": len(data)}


def check_configuration(configuration):
    require(set(configuration) == {"app", "loader", "package_identity"}, "Invalid build configuration")
    for unit in ("app", "loader"):
        item = configuration[unit]
        require(item.get("identity") == receipt.IDENTITIES[unit], f"Wrong compiled {unit} identity")
        require(set(item.get("flags", {})) == set(receipt.FLAGS) and
                all(type(value) is int and value in (0, 1) for value in item["flags"].values()),
                f"Missing or invalid preprocessed {unit} flags")
    for name, value in receipt.REQUIRED_FLAGS.items():
        require(configuration["app"]["flags"][name] == value, f"Candidate requires {name}={value}; rebuild with --with-updater")
    for name, value in {"JAMBOX_PROTO": 1, "FELUCCA_OTA": 1, "FELUCCA_OTA_DRYRUN": 0,
                        "FELUCCA_OTA_RAMONLY": 0, "FELUCCA_PERSIST": 0, "JAMBOX_RECOVERY": 0}.items():
        require(configuration["loader"]["flags"][name] == value, f"Wrong loader flag {name}")
    require(configuration["app"].get("version") == "JamBoxFM " + receipt.VERSION, "Wrong compiled JamBox version")
    require(configuration["package_identity"] == receipt.IDENTITIES["package"], "Wrong package identity")


def verify_build(root, build, toolchain, sdk):
    """Verify a normal candidate; recovery-enabled builds are always refused."""
    return _verify_build(root, build, toolchain, sdk, check_configuration)


def _verify_build(root, build, toolchain, sdk, configuration_validator):
    """Verify source, tool, artifact, ELF and canonical container consistency."""
    receipt.jam_identity(root)
    record_path = build / "device-build-receipt.json"
    require(record_path.is_file() and not record_path.is_symlink(), "Missing build receipt; rebuild the updater target")
    record = json.loads(record_path.read_bytes())
    require(record.get("schema_version") == 1 and record.get("kind") == "jambox-device-build", "Unsupported build receipt")
    configuration_validator(record["configuration"])
    inputs = record["inputs_sha256"]
    dependencies = receipt.dependency_inputs(root, build)
    expected_inputs = (dependencies - set(receipt.GENERATED)) | receipt.generator_inputs(root)
    require(set(inputs) == expected_inputs, "Stale or incomplete build-input receipt")
    require(receipt.hashes(root, inputs) == inputs, "Stale source or build tools; rebuild before packaging")
    generated = record["generated_inputs_sha256"]
    require(set(generated) == dependencies & set(receipt.GENERATED) and
            receipt.hashes(root, generated) == generated, "Stale or tampered generated headers")
    expected_artifacts = {*receipt.ARTIFACTS, "felucca.fwsc"}
    require(set(record["artifacts"]) == expected_artifacts, "Unexpected or incomplete artifact receipt")
    for name, item in record["artifacts"].items():
        path = build / name
        require(not path.is_symlink() and path.is_file() and path.resolve().is_relative_to(build.resolve()),
                f"Missing or unsafe build artifact: {name}")
        require(item == {"sha256": receipt.file_sha(path), "size_bytes": path.stat().st_size},
                f"Tampered or mixed build artifact: {name}")
    external = receipt.external_inputs(toolchain, sdk, strict=True)
    require(record["toolchain"] == external["compiler"] and record["sdk"] == external["sdk"],
            "Build compiler/SDK provenance differs from the current verified tools")
    require(isinstance(record.get("commands"), list) and record["commands"], "Missing actual build commands")
    memory = report_device.report(build, toolchain)
    require(memory["ok"], "Device memory/ELF gate failed: " + "; ".join(memory["errors"]))
    # The existing builder fixes every CRC, field, offset, region and chip key.
    # Full byte equality also rejects adjusted CRCs, unknown entries and padding.
    previous_sdk = fm1pkg_make.SDK
    try:
        fm1pkg_make.SDK = sdk
        app = (build / "felucca.bin").read_bytes()
        ota = (build / "loader/ota.bin").read_bytes()
        canonical = fm1pkg_make.ufw(fm1pkg_make.flash_image(app, 0x980F), ota, receipt.IDENTITIES["package"])
    finally:
        fm1pkg_make.SDK = previous_sdk
    require((build / "felucca.fwsc").read_bytes() == canonical, "Package differs from canonical verified app/loader, chip key or SDK")
    require(receipt.IDENTITIES["app"].encode() in app and ("JamBoxFM " + receipt.VERSION).encode() in app,
            "Compiled application identity/version bytes are missing")
    require(receipt.IDENTITIES["loader"].encode() in (build / "loader/loader.bin").read_bytes(),
            "Compiled loader identity bytes are missing")
    # Do not put a machine-specific directory into otherwise portable evidence.
    memory["build_dir"] = "build"
    return record, canonical, memory


def source_archive(root, record):
    require(root.resolve() == source_package.ROOT.resolve(), "Source selector root differs from package root")
    entries = {}
    for path in source_package.source_files():
        relative = path.relative_to(root)
        # Licensed fonts/samples are selected explicitly by the shared exporter.
        # Nested local SDK/toolchain checkouts never become corresponding source.
        require(not (set(part.lower() for part in relative.parts) &
                     {"sdk", "toolchain", "toolchains", "hiphop-pack", "logo", "logos", "screens"}),
                f"Unsafe source-export selection: {relative}")
        require(relative.as_posix() != "assets/icons.png" and not relative.as_posix().startswith("docs/firmware/"),
                f"Reserved artifact in source export: {relative}")
        entries[relative.as_posix()] = path.read_bytes()
    for name, digest in record["inputs_sha256"].items():
        require(name in entries and receipt.sha(entries[name]) == digest,
                f"Corresponding source is missing or changed since build: {name}")
    hashes = {name: receipt.sha(data) for name, data in sorted(entries.items())}
    entries["SOURCE-MANIFEST.json"] = receipt.json_bytes(hashes)
    return source_package.make_zip({"jamboxfm-source/" + name: data for name, data in entries.items()}), hashes


def license_files(root, sdk):
    files = {target: (root / original).read_bytes() for original, target in source_package.NOTICES.items()}
    for name in ("phys_dsp.c", "phys_symp.c"):
        text = (root / "firmware/src" / name).read_text()
        files["licenses/" + name + "-notice.txt"] = (text[:text.index("*/") + 2] + "\n").encode()
    files["licenses/AC79-SDK-LICENSE.txt"] = (sdk / "LICENSE").read_bytes()
    files["licenses/AC79-SDK-README.md"] = (sdk / "README.md").read_bytes()
    return files


def package(root=ROOT, build=BUILD, output=OUTPUT, toolchain=None, sdk=None):
    require(toolchain is not None and sdk is not None, "Verified compiler and SDK paths are required")
    record, firmware, memory = verify_build(root, build, toolchain, sdk)
    source_zip, source_hashes = source_archive(root, record)
    licenses = license_files(root, sdk)
    receipt_bytes = receipt.json_bytes(record)
    memory_bytes = receipt.json_bytes(memory)
    manifest = {"schema_version": 1, "family": "jamboxfm", "version": receipt.VERSION,
        "release_enabled": False, "hardware_validated": False,
        "target": {"model": "M-VAVE FM-1", "soc": "AC791N", "platform": "WL82", "flash_bytes": 1048576,
                   "flash_jedec": "0x856014", "chip_key": "0x980f"},
        "identities": receipt.IDENTITIES, "firmware": descriptor(FIRMWARE, firmware),
        "source": {**descriptor(SOURCE, source_zip), **{key: record["source"][key] for key in ("revision", "dirty")}},
        "build": {"receipt_file": "build-receipt.json", "receipt_sha256": receipt.sha(receipt_bytes),
                  "flags": record["configuration"]["app"]["flags"], "memory_report_file": "memory.json",
                  "memory_report_sha256": receipt.sha(memory_bytes)},
        "transitions": {"mode": "simulation-only", "approved": [], "simulated": TRANSITIONS},
        "return_firmware": SLOOP_26, "format_validation": FORMAT_EVIDENCE,
        "licenses": [descriptor(name, data) for name, data in sorted(licenses.items())]}
    files = {FIRMWARE: firmware, SOURCE: source_zip, "manifest.json": receipt.json_bytes(manifest),
             "build-receipt.json": receipt_bytes, "memory.json": memory_bytes, **licenses}
    files["README.txt"] = ("JamBoxFM local software candidate " + receipt.VERSION + "\n\n"
        "This candidate manifest records software validation only; hardware_validated remains false.\n"
        "It does not itself authorize an installation. A separately packaged trial.json can bind\n"
        "these exact bytes to the experimental factory-V15 install or normal JamBox update workflow.\n"
        "That experiment does not establish hardware reliability or interrupted-update recovery.\n"
        "manifest.json identifies the firmware, source, build evidence and pinned return package.\n"
        "The return Sloop package is identified by hash only and is not included.\n"
        "Source/toolchain hashes are reproducibility records, not digital signatures.\n"
        "Rebuild: bash tools/cloud/build_device.sh --with-updater\n"
        "Package: python3 tools/package_jam_device.py --no-build\n").encode()
    files["SHA256SUMS"] = "".join(f"{receipt.sha(data)}  {name}\n" for name, data in sorted(files.items())).encode()
    # Recheck after reading source and notices, before replacing a prior bundle.
    require(receipt.hashes(root, source_hashes) == source_hashes, "Source changed while packaging; retry")
    require(receipt.external_inputs(toolchain, sdk, strict=False) == {"compiler": record["toolchain"], "sdk": record["sdk"]},
            "Compiler or SDK changed while packaging")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".jambox-device-", dir=output.parent) as temporary:
        stage = Path(temporary) / "bundle"
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        require(not output.is_symlink(), "Refusing a symlink output directory")
        if output.exists():
            shutil.rmtree(output)
        stage.rename(output)
    print(f"Local software candidate: {output}; {len(firmware):,} firmware bytes; SHA256 {receipt.sha(firmware)}")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true", help="verify and package the existing updater-enabled target")
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")))
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")))
    args = parser.parse_args()
    try:
        if not args.no_build:
            subprocess.run(["bash", "tools/cloud/build_device.sh", "--with-updater"], cwd=ROOT,
                           env={**os.environ, "JIELI_TOOLCHAIN": str(args.toolchain), "AC79_SDK": str(args.sdk)}, check=True)
        package(toolchain=args.toolchain.resolve(), sdk=args.sdk.resolve())
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Candidate packaging refused: {exc}\n")


if __name__ == "__main__":
    main()
