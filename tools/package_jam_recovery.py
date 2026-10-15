#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the isolated manual-saving FM-1 trial, its source and build evidence.

Nothing connects to an instrument or publishes a site. Only the explicit
--with-recovery target is accepted. The normal device packager still refuses
that target. This bundle is limited to updating an already-running JamBox.
"""
from __future__ import annotations

import argparse
import base64
import copy
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

import device_build_receipt as receipt
import package_jam_device as normal
import publish_jam_web as publication

ROOT = normal.ROOT
BUILD = normal.BUILD
OUTPUT = BUILD / "jambox-recovery"
FIRMWARE = f"jamboxfm-recovery-{receipt.VERSION}.fwsc"
SOURCE = normal.SOURCE
TRANSITIONS = ["jambox-normal-to-jambox"]
RECOVERY = {"schema_version": 1, "automatic_saving": False,
            "requires_storage_setup": True, "legacy_storage_replaced": True}
REQUIRED_FLAGS = {**receipt.REQUIRED_FLAGS, "JAMBOX_RECOVERY": 1}
LICENSE_FILES = publication.TRIAL_LICENSE_FILES
BUNDLE_FILES = frozenset({FIRMWARE, SOURCE, "manifest.json", "trial.json",
    "build-receipt.json", "memory.json", "README.txt", "SHA256SUMS"}) | LICENSE_FILES
require = receipt.require


def check_configuration(configuration):
    """Apply all normal gates, changing only the explicit recovery requirement."""
    require(isinstance(configuration, dict) and isinstance(configuration.get("app"), dict) and
            isinstance(configuration["app"].get("flags"), dict), "Invalid recovery build configuration")
    recovery_flag = configuration.get("app", {}).get("flags", {}).get("JAMBOX_RECOVERY")
    require(type(recovery_flag) is int and recovery_flag == 1,
            "Recovery candidate requires JAMBOX_RECOVERY=1; rebuild with --with-recovery")
    reference = copy.deepcopy(configuration)
    reference["app"]["flags"]["JAMBOX_RECOVERY"] = 0
    normal.check_configuration(reference)


def verify_build(root, build, toolchain, sdk):
    """Reuse the complete ELF/provenance/container gates with recovery policy."""
    return normal._verify_build(root, build, toolchain, sdk, check_configuration)


def trial_descriptor(manifest_bytes, firmware):
    return {"schema_version": 1, "kind": "development-recovery-install",
        "hardware_validated": False, "release_enabled": False,
        "transitions": TRANSITIONS, "manifest_sha256": receipt.sha(manifest_bytes),
        "firmware_sha256": receipt.sha(firmware)}


def _validate_candidate_format(manifest, firmware):
    # Execute reviewed repository policy, never JavaScript from a proposed ZIP.
    script = """import fs from 'node:fs';
import { prepareRecoveryCandidate } from './web/jam-recovery-policy.js';
const input = JSON.parse(fs.readFileSync(0, 'utf8'));
await prepareRecoveryCandidate(input.manifest, new Uint8Array(Buffer.from(input.firmware, 'base64')));
"""
    result = subprocess.run(["node", "--input-type=module", "--eval", script], cwd=ROOT,
        input=json.dumps({"manifest": manifest, "firmware": base64.b64encode(firmware).decode("ascii")}),
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    require(result.returncode == 0, "Recovery candidate format validation failed: " + result.stderr.strip())


def validate_bundle_files(files):
    """Check an immutable bundle snapshot without build tools or device access.

    These are consistency checks against the reviewed repository policy, not a
    second target rebuild, signature or claim that hardware has been tested.
    """
    require(set(files) == BUNDLE_FILES, "Recovery bundle must contain exactly its approved files")
    require(all(isinstance(body, bytes) and len(body) <= 64 * 1024 * 1024 for body in files.values()),
            "Invalid or oversized recovery bundle file")
    publication.validate_hashes(files, "Recovery candidate")
    manifest = publication.json_object(files["manifest.json"], "recovery manifest")
    trial = publication.json_object(files["trial.json"], "recovery trial descriptor")
    expected_trial = trial_descriptor(files["manifest.json"], files[FIRMWARE])
    require(trial == expected_trial and type(trial.get("schema_version")) is int and
            trial.get("hardware_validated") is False and trial.get("release_enabled") is False,
            "Recovery trial descriptor or candidate binding is invalid")
    _validate_candidate_format(manifest, files[FIRMWARE])

    def artifact(name, digest, size=None):
        require(name in files and receipt.sha(files[name]) == digest and
                (size is None or len(files[name]) == size), "Recovery artifact descriptor mismatch: " + name)
        return files[name]

    for item in (manifest["firmware"], manifest["source"], *manifest["licenses"]):
        artifact(item["file"], item["sha256"], item["size_bytes"])
    require({item["file"] for item in manifest["licenses"]} == LICENSE_FILES,
            "Recovery licenses must match the exact publication allowlist")
    evidence = manifest["build"]
    record = publication.json_object(artifact(evidence["receipt_file"], evidence["receipt_sha256"]), "recovery receipt")
    memory = publication.json_object(artifact(evidence["memory_report_file"], evidence["memory_report_sha256"]), "recovery memory report")
    require(record.get("schema_version") == 1 and record.get("kind") == "jambox-device-build",
            "Unsupported recovery build receipt")
    check_configuration(record.get("configuration", {}))
    artifacts = record.get("artifacts", {})
    require(set(artifacts) == {*receipt.ARTIFACTS, "felucca.fwsc"} and all(
        isinstance(item, dict) and set(item) == {"sha256", "size_bytes"} and
        isinstance(item["sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) and
        type(item["size_bytes"]) is int and item["size_bytes"] > 0 for item in artifacts.values()),
        "Invalid or incomplete recovery artifact receipt")
    require(record["configuration"]["app"]["flags"] == evidence["flags"] and
            record.get("artifacts", {}).get("felucca.fwsc") == {
                "sha256": manifest["firmware"]["sha256"], "size_bytes": manifest["firmware"]["size_bytes"]},
            "Recovery receipt does not describe this firmware/configuration")
    require(memory.get("schema_version") == 1 and memory.get("ok") is True and memory.get("errors") == [] and
            memory.get("app") and memory.get("loader") and
            memory.get("ota", {}).get("headers_crc_body_crc_and_loader_match") is True,
            "Recovery memory/ELF evidence is incomplete or failed")
    for field, name in {"app_bin": "felucca.bin", "app_elf": "felucca.elf", "loader_bin": "loader/loader.bin",
                        "loader_elf": "loader/loader.elf", "ota_bin": "loader/ota.bin"}.items():
        require(memory.get("sha256", {}).get(field) == record.get("artifacts", {}).get(name, {}).get("sha256"),
                "Recovery memory report and build receipt disagree: " + name)
    source = publication.source_contents(files[SOURCE])
    required = {"firmware/src/jam_identity.h", "firmware/src/jam_recovery_runtime.c",
        "firmware/experimental/recovery/jr_store.c", "firmware/experimental/recovery/jr_codec.c",
        "firmware/loader/loader.c", "firmware/loader/ldr_core.c", "tools/package_jam_recovery.py",
        "tools/device_build_receipt.py", "tools/cloud/build_device.sh"}
    require(required <= source.keys() and record.get("inputs_sha256"), "Recovery corresponding device source is missing")
    for name, digest in record["inputs_sha256"].items():
        require(name in source and receipt.sha(source[name]) == digest,
                "Recovery source differs from the build receipt: " + name)
    require(all(manifest["source"][field] == record.get("source", {}).get(field) for field in ("revision", "dirty")),
            "Recovery source revision differs from the build receipt")
    for name in ("LICENSE", "LICENSING.md"):
        require(source.get(name) == files[name], "Recovery " + name + " differs from its corresponding source")
    return manifest


def package(root=ROOT, build=BUILD, output=OUTPUT, toolchain=None, sdk=None):
    require(toolchain is not None and sdk is not None, "Verified compiler and SDK paths are required")
    record, firmware, memory = verify_build(root, build, toolchain, sdk)
    source_zip, source_hashes = normal.source_archive(root, record)
    licenses = normal.license_files(root, sdk)
    require(set(licenses) == LICENSE_FILES, "Unexpected recovery license selection")
    receipt_bytes, memory_bytes = receipt.json_bytes(record), receipt.json_bytes(memory)
    manifest = {"schema_version": 1, "kind": "development-recovery-candidate", "family": "jamboxfm",
        "version": receipt.VERSION, "release_enabled": False, "hardware_validated": False,
        "target": {"model": "M-VAVE FM-1", "soc": "AC791N", "platform": "WL82", "flash_bytes": 1048576,
                   "flash_jedec": "0x856014", "chip_key": "0x980f"},
        "identities": receipt.IDENTITIES, "firmware": normal.descriptor(FIRMWARE, firmware),
        "source": {**normal.descriptor(SOURCE, source_zip),
                   **{key: record["source"][key] for key in ("revision", "dirty")}},
        "build": {"receipt_file": "build-receipt.json", "receipt_sha256": receipt.sha(receipt_bytes),
                  "flags": record["configuration"]["app"]["flags"], "memory_report_file": "memory.json",
                  "memory_report_sha256": receipt.sha(memory_bytes)},
        "transitions": {"mode": "development-trial", "approved": [], "simulated": TRANSITIONS},
        "recovery": RECOVERY, "return_firmware": normal.SLOOP_26, "format_validation": normal.FORMAT_EVIDENCE,
        "licenses": [normal.descriptor(name, data) for name, data in sorted(licenses.items())]}
    manifest_bytes = receipt.json_bytes(manifest)
    files = {FIRMWARE: firmware, SOURCE: source_zip, "manifest.json": manifest_bytes,
             "trial.json": receipt.json_bytes(trial_descriptor(manifest_bytes, firmware)),
             "build-receipt.json": receipt_bytes, "memory.json": memory_bytes, **licenses}
    files["README.txt"] = ("JamBoxFM isolated manual-saving trial " + receipt.VERSION + "\n\n"
        "Development only: hardware_validated=false, release_enabled=false. Automatic saving is off.\n"
        "Install only through the separate recovery trial page from a running JamBox application.\n"
        "Factory, rescue and loader-only starting states are not accepted by this trial.\n"
        "Keep a fresh full-flash backup. Initial storage setup replaces legacy projects and USER sounds.\n"
        "Factory sounds and protected calibration are retained. Save now requires stopped playback.\n"
        "The stable source and normal installer are independent of this candidate.\n"
        "SHA256SUMS and source/tool hashes record consistency; they are not digital signatures.\n"
        "Build: bash tools/cloud/build_device.sh --with-recovery\n"
        "Package: python3 tools/package_jam_recovery.py --no-build\n").encode()
    files["SHA256SUMS"] = "".join(f"{receipt.sha(data)}  {name}\n" for name, data in sorted(files.items())).encode()
    validate_bundle_files(files)
    require(receipt.hashes(root, source_hashes) == source_hashes, "Source changed while packaging; retry")
    require(receipt.external_inputs(toolchain, sdk, strict=False) == {"compiler": record["toolchain"], "sdk": record["sdk"]},
            "Compiler or SDK changed while packaging")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".jambox-recovery-", dir=output.parent) as temporary:
        stage = Path(temporary) / "bundle"
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        require(not output.is_symlink(), "Refusing a symlink output directory")
        if output.exists():
            shutil.rmtree(output)
        stage.rename(output)
    print(f"Local recovery trial: {output}; {len(firmware):,} firmware bytes; SHA256 {receipt.sha(firmware)}")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true", help="verify and package the existing recovery-enabled target")
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("JIELI_TOOLCHAIN", "/workspace/toolchains/jieli/toolchain")))
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("AC79_SDK", "/workspace/deps/ac79-sdk")))
    args = parser.parse_args()
    try:
        if not args.no_build:
            subprocess.run(["bash", "tools/cloud/build_device.sh", "--with-recovery"], cwd=ROOT,
                env={**os.environ, "JIELI_TOOLCHAIN": str(args.toolchain), "AC79_SDK": str(args.sdk)}, check=True)
        package(toolchain=args.toolchain.resolve(), sdk=args.sdk.resolve())
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired,
            publication.DeploymentError) as exc:
        parser.exit(1, f"Recovery candidate packaging refused: {exc}\n")


if __name__ == "__main__":
    main()
