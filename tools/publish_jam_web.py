#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate and stage JamBox's packaged browser site for GitHub Pages.

Default: verify build/jambox-web, make an isolated local deployment commit and
run git push --dry-run. --publish performs a normal, non-forced push to only
anthonysprints/jamboxfm's gh-pages branch. It does not enable GitHub Pages settings.
The source checkout, its index and its branches are never changed.
"""
from __future__ import annotations

import argparse
import base64
from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
from typing import Iterator
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "build/jambox-web"
DESTINATION = "https://github.com/anthonysprints/jamboxfm.git"
BRANCH = "refs/heads/gh-pages"
EXPECTED_FILES = frozenset({
    ".nojekyll", "index.html", "LICENSE", "LICENSING.md", "LICENSES.html", "WEB_EMULATOR.md",
    "jambox-emulator.html", "jambox-source.zip", "SHA256SUMS",
    "emu/index.html", "emu/style.css", "emu/app.js", "emu/worklet.js", "emu/jam-emu.wasm",
    "licenses/Apache-2.0-msfa.txt", "licenses/Terminus-LICENSE.txt", "licenses/CC0-ATTRIBUTION.txt",
    "licenses/CC0-CREDITS.txt", "licenses/FUKIAI-LICENSE.txt", "licenses/phys_dsp.c-notice.txt",
    "licenses/phys_symp.c-notice.txt",
    "device/index.html", "device/style.css", "device/app.js",
    "fm1ota.js", "fm1pkg.js", "jam-device-policy.js",
})
EXPECTED_DIRECTORIES = frozenset({"emu", "licenses", "device"})
TRIAL_RUNTIME_FILES = frozenset({"install/index.html", "install/style.css", "install/app.js", "jam-trial-install.js"})
TRIAL_LICENSE_FILES = frozenset({"LICENSE", "LICENSING.md", "licenses/Apache-2.0-msfa.txt",
    "licenses/Terminus-LICENSE.txt", "licenses/CC0-ATTRIBUTION.txt", "licenses/CC0-CREDITS.txt",
    "licenses/FUKIAI-LICENSE.txt", "licenses/phys_dsp.c-notice.txt", "licenses/phys_symp.c-notice.txt",
    "licenses/AC79-SDK-LICENSE.txt", "licenses/AC79-SDK-README.md"})
TRIAL_BUNDLE_FILES = TRIAL_LICENSE_FILES | {"jamboxfm-0.1.0-dev.fwsc", "jamboxfm-source.zip",
    "manifest.json", "build-receipt.json", "memory.json", "README.txt", "SHA256SUMS"}
TRIAL_FILES = EXPECTED_FILES | TRIAL_RUNTIME_FILES | {"firmware/" + name for name in TRIAL_BUNDLE_FILES | {"trial.json"}}
TRIAL_DIRECTORIES = EXPECTED_DIRECTORIES | {"install", "firmware", "firmware/licenses"}
SOURCE_PREFIX = "jamboxfm-source/"
SOURCE_REQUIRED = frozenset({
    "LICENSE", "LICENSING.md", "firmware/host/jam_emu.c", "firmware/src/jam_ui.c",
    "tools/build_jam_emu.py", "tools/package_jam_emu.py", "web/emu/index.html",
    "web/emu/style.css", "web/emu/app.js", "web/emu/worklet.js", "docs/WEB_EMULATOR.md",
    "web/device/index.html", "web/device/style.css", "web/device/app.js",
    "web/fm1ota.js", "web/fm1pkg.js", "web/jam-device-policy.js",
})
SOURCE_ROOTS = frozenset({"firmware", "tools", "tests", "web", "LICENSES", "assets", "docs"})
SOURCE_ROOT_FILES = frozenset({
    "README.md", "BUILDING.md", "LICENSING.md", "LICENSE", "PRODUCT_BRIEF.md", "SLOOP.md",
    "GUIDE.md", "DEMARRAGE-RAPIDE-FR.md", "build.sh", "build-sloop.ps1", "INSTALL-SLOOP.bat",
    "OPEN-EDITOR.bat", ".gitignore", ".gitattributes", "SOURCE-MANIFEST.json",
})
BLOCKED_SOURCE_PARTS = frozenset({
    ".git", ".aws", ".ssh", ".env", ".npmrc", ".netrc", "node_modules", "__pycache__",
    ".cache", ".venv", "build", "data", "private", "secrets", "credentials", "id_rsa", "id_ed25519",
})
BLOCKED_BINARY_SUFFIXES = frozenset({".fwsc", ".bin", ".elf", ".wasm", ".pem", ".key", ".p12", ".pfx"})
GIT_LOCATION_ENV = frozenset({
    "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR", "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_PREFIX", "GIT_NAMESPACE", "GIT_SHALLOW_FILE",
    "GIT_QUARANTINE_PATH", "GIT_REPLACE_REF_BASE",
})
GIT_OPTIONS = ["-c", "core.hooksPath=/dev/null", "-c", "core.fsmonitor=false",
               "-c", "core.autocrlf=false", "-c", "core.attributesFile=/dev/null"]


class DeploymentError(RuntimeError):
    """An unsafe package, unavailable prerequisite, or Git operation failed."""


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_object(data: bytes, description: str) -> dict:
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise DeploymentError(f"Duplicate key in {description}: {key}")
            result[key] = value
        return result
    try:
        result = json.loads(data, object_pairs_hook=unique)
    except (ValueError, UnicodeError) as error:
        raise DeploymentError(f"Invalid {description}: {error}") from error
    if not isinstance(result, dict):
        raise DeploymentError(f"{description} must be an object.")
    return result


def source_contents(data: bytes) -> dict[str, bytes]:
    """Check corresponding source without extracting it or executing its tools.

    Archive paths, checksums and source presence are checked. No archive code is
    executed. This is not a claim of independently rebuilding its binary.
    """
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            entries = archive.infolist()
            if len(entries) > 10000 or sum(entry.file_size for entry in entries) > 256 * 1024 * 1024:
                raise DeploymentError("Corresponding source archive exceeds the supported size.")
            content = {}
            for entry in entries:
                name = entry.filename
                path = PurePosixPath(name)
                if (not name.startswith(SOURCE_PREFIX) or path.is_absolute() or "\\" in name or
                        any(part in ("", ".", "..") for part in name.split("/")) or entry.is_dir()):
                    raise DeploymentError(f"Unsafe source archive path: {name!r}")
                mode = entry.external_attr >> 16
                if stat.S_ISLNK(mode) or (stat.S_IFMT(mode) not in (0, stat.S_IFREG)) or entry.flag_bits & 1:
                    raise DeploymentError(f"Unsupported source archive entry: {name!r}")
                relative = name.removeprefix(SOURCE_PREFIX)
                parts = PurePosixPath(relative).parts
                if (set(part.lower() for part in parts) & BLOCKED_SOURCE_PARTS or
                        any(part.lower().startswith('.env.') for part in parts) or
                        PurePosixPath(relative).suffix.lower() in BLOCKED_BINARY_SUFFIXES):
                    raise DeploymentError(f"Private or binary material in source archive: {relative}")
                if ((len(parts) == 1 and relative not in SOURCE_ROOT_FILES) or
                        (len(parts) > 1 and parts[0] not in SOURCE_ROOTS)):
                    raise DeploymentError(f"Unexpected corresponding source location: {relative}")
                if (relative in {"assets/icons.png", "docs/panel.jpg"} or
                        (parts[0] == "assets" and relative != "assets/icons.json" and
                         not relative.startswith(("assets/fonts/", "assets/samples-cc0/"))) or
                        (parts[0] == "docs" and not
                         (len(parts) == 2 and relative.endswith(".md") or
                          len(parts) == 3 and parts[1] == "upstream" and relative.endswith((".md", ".json"))))):
                    raise DeploymentError(f"Excluded asset or deployed document in source archive: {relative}")
                if relative in content:
                    raise DeploymentError(f"Duplicate source archive path: {relative}")
                content[relative] = archive.read(entry)
    except (zipfile.BadZipFile, RuntimeError) as error:
        if isinstance(error, DeploymentError):
            raise
        raise DeploymentError(f"Cannot verify corresponding source archive: {error}") from error
    manifest_data = content.pop("SOURCE-MANIFEST.json", None)
    if manifest_data is None or not SOURCE_REQUIRED <= content.keys():
        raise DeploymentError("Corresponding source or SOURCE-MANIFEST.json is missing.")
    manifest = json_object(manifest_data, "SOURCE-MANIFEST.json")
    if set(manifest) != set(content):
        raise DeploymentError("Source manifest does not describe every archived source file exactly once.")
    for name, body in content.items():
        if manifest[name] != sha(body):
            raise DeploymentError(f"Source checksum mismatch: {name}")
    return content


def validate_source(data: bytes, files: dict[str, bytes]) -> int:
    content = source_contents(data)
    for name in ("style.css", "app.js", "worklet.js"):
        if content[f"web/emu/{name}"] != files[f"emu/{name}"]:
            raise DeploymentError(f"Published {name} differs from its corresponding source.")
    for name in ("device/index.html", "device/style.css", "device/app.js",
                 "fm1ota.js", "fm1pkg.js", "jam-device-policy.js"):
        if content[f"web/{name}"] != files[name]:
            raise DeploymentError(f"Published {name} differs from its corresponding source.")
    if content["LICENSE"] != files["LICENSE"]:
        raise DeploymentError("Published LICENSE differs from corresponding source.")
    if "firmware/trial.json" in files:
        for name in TRIAL_RUNTIME_FILES:
            if content.get("web/" + name) != files[name]:
                raise DeploymentError(f"Published {name} differs from its corresponding source.")
    return len(content)


def validate_hashes(files: dict[str, bytes], label: str = "Publish") -> None:
    try:
        lines = files["SHA256SUMS"].decode("ascii").splitlines()
    except UnicodeError as error:
        raise DeploymentError(f"{label} SHA256SUMS is not ASCII.") from error
    hashes = {}
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match or match[2] in hashes:
            raise DeploymentError(f"{label} SHA256SUMS has an invalid or duplicate entry.")
        hashes[match[2]] = match[1]
    if set(hashes) != files.keys() - {"SHA256SUMS"}:
        raise DeploymentError(f"{label} SHA256SUMS must cover exactly the other packaged files.")
    for name, digest in hashes.items():
        if sha(files[name]) != digest:
            raise DeploymentError(f"{label} checksum mismatch: {name}; rebuild the package.")


def validate_trial(files: dict[str, bytes]) -> None:
    """Validate the opt-in install/update trial without promoting the candidate.

    The packager independently verifies the local ELF and pinned SDK/compiler.
    Publication rechecks the immutable package, source, receipt and their links;
    it does not claim a second independent target rebuild or hardware execution.
    """
    bundle = {name: files["firmware/" + name] for name in TRIAL_BUNDLE_FILES}
    validate_hashes(bundle, "Candidate")
    manifest = json_object(bundle["manifest.json"], "candidate manifest")
    trial = json_object(files["firmware/trial.json"], "experimental trial descriptor")
    expected_trial = {"schema_version": 2, "kind": "experimental-install", "hardware_validated": False,
        "transitions": ["factory-v15-to-jambox", "jambox-normal-to-jambox"],
        "manifest_sha256": sha(bundle["manifest.json"]),
        "firmware_sha256": sha(bundle["jamboxfm-0.1.0-dev.fwsc"])}
    if trial != expected_trial or type(trial.get("schema_version")) is not int or trial.get("hardware_validated") is not False:
        raise DeploymentError("Experimental trial descriptor or candidate binding is invalid.")
    # Run the reviewed validator in the checkout, never JavaScript from a ZIP or
    # the proposed publication directory. Input is JSON through stdin only.
    script = """import fs from 'node:fs';
import { prepareCandidate } from './web/jam-device-policy.js';
const input = JSON.parse(fs.readFileSync(0, 'utf8'));
await prepareCandidate(input.manifest, new Uint8Array(Buffer.from(input.firmware, 'base64')));
"""
    try:
        result = subprocess.run(["node", "--input-type=module", "--eval", script], cwd=ROOT,
            input=json.dumps({"manifest": manifest, "firmware": base64.b64encode(bundle["jamboxfm-0.1.0-dev.fwsc"]).decode("ascii")}),
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise DeploymentError(f"Cannot run the candidate format validator: {error}") from error
    if result.returncode:
        raise DeploymentError("Candidate format validation failed: " + result.stderr.strip())

    def artifact(name, digest, size=None):
        if name not in bundle or sha(bundle[name]) != digest or (size is not None and len(bundle[name]) != size):
            raise DeploymentError(f"Candidate artifact descriptor mismatch: {name}")
        return bundle[name]

    for item in (manifest["firmware"], manifest["source"], *manifest["licenses"]):
        artifact(item["file"], item["sha256"], item["size_bytes"])
    if {item["file"] for item in manifest["licenses"]} != TRIAL_LICENSE_FILES:
        raise DeploymentError("Candidate licenses must match the exact publication allowlist.")
    evidence = manifest["build"]
    record = json_object(artifact(evidence["receipt_file"], evidence["receipt_sha256"]), "build receipt")
    memory = json_object(artifact(evidence["memory_report_file"], evidence["memory_report_sha256"]), "memory report")
    config = record.get("configuration", {})
    if (record.get("schema_version") != 1 or record.get("kind") != "jambox-device-build" or
            config.get("app", {}).get("identity") != "FM-1_700" or
            config.get("app", {}).get("version") != "JamBoxFM " + manifest["version"] or
            config.get("loader", {}).get("identity") != "ota-FM-1_700" or
            config.get("package_identity") != "FM-1_700" or
            config.get("app", {}).get("flags") != evidence["flags"] or
            record.get("artifacts", {}).get("felucca.fwsc") != {
                "sha256": manifest["firmware"]["sha256"], "size_bytes": manifest["firmware"]["size_bytes"]}):
        raise DeploymentError("Candidate build receipt does not describe this firmware/configuration.")
    loader_flags = config.get("loader", {}).get("flags", {})
    if any(loader_flags.get(name) != value for name, value in {"JAMBOX_PROTO": 1, "FELUCCA_OTA": 1,
            "FELUCCA_OTA_DRYRUN": 0, "FELUCCA_OTA_RAMONLY": 0, "FELUCCA_PERSIST": 0}.items()):
        raise DeploymentError("Candidate loader flags differ from the experimental build.")
    if (memory.get("schema_version") != 1 or memory.get("ok") is not True or memory.get("errors") != [] or
            not memory.get("app") or not memory.get("loader") or
            memory.get("ota", {}).get("headers_crc_body_crc_and_loader_match") is not True):
        raise DeploymentError("Candidate memory/ELF evidence is incomplete or failed.")
    for field, name in {"app_bin": "felucca.bin", "app_elf": "felucca.elf", "loader_bin": "loader/loader.bin",
                        "loader_elf": "loader/loader.elf", "ota_bin": "loader/ota.bin"}.items():
        if memory.get("sha256", {}).get(field) != record.get("artifacts", {}).get(name, {}).get("sha256"):
            raise DeploymentError(f"Candidate memory report and build receipt disagree: {name}")
    source = source_contents(bundle["jamboxfm-source.zip"])
    required = {"firmware/src/jam_identity.h", "firmware/loader/loader.c", "firmware/loader/ldr_core.c",
        "tools/package_jam_device.py", "tools/device_build_receipt.py", "tools/cloud/build_device.sh"}
    if not required <= source.keys() or not record.get("inputs_sha256"):
        raise DeploymentError("Candidate corresponding device source is missing.")
    for name, digest in record["inputs_sha256"].items():
        if name not in source or sha(source[name]) != digest:
            raise DeploymentError(f"Candidate source differs from the build receipt: {name}")
    if any(manifest["source"][field] != record.get("source", {}).get(field) for field in ("revision", "dirty")):
        raise DeploymentError("Candidate source revision differs from the build receipt.")
    for name in ("LICENSE", "LICENSING.md"):
        if source.get(name) != bundle[name]:
            raise DeploymentError(f"Candidate {name} differs from its corresponding source.")


def validate_site(site: Path) -> dict[str, bytes]:
    """Return a checksum-verified snapshot of the exact approved public files."""
    if not site.is_absolute():
        site = site.absolute()
    for component in (site, *site.parents):
        if component.is_symlink():
            raise DeploymentError(f"Publish root uses a symlink: {component}")
    if not site.is_dir():
        raise DeploymentError("Packaged site is missing; run python3 tools/package_jam_emu.py first.")
    found, directories = set(), set()
    for path in site.rglob("*"):
        name = path.relative_to(site).as_posix()
        mode = path.lstat().st_mode
        if stat.S_ISLNK(mode):
            raise DeploymentError(f"Publish root contains a symlink: {name}")
        if stat.S_ISDIR(mode):
            directories.add(name)
        elif stat.S_ISREG(mode):
            found.add(name)
        else:
            raise DeploymentError(f"Publish root contains a non-regular file: {name}")
    with_trial = "firmware/trial.json" in found
    expected = TRIAL_FILES if with_trial else EXPECTED_FILES
    expected_directories = TRIAL_DIRECTORIES if with_trial else EXPECTED_DIRECTORIES
    if found != expected or directories != expected_directories:
        unexpected = sorted((found - expected) | (directories - expected_directories))
        missing = sorted(expected - found)
        raise DeploymentError(f"Publish root must contain exactly its {len(expected)} packaged files. Missing: {missing}; unexpected: {unexpected}")
    if any((site / name).stat().st_size > 64 * 1024 * 1024 for name in found):
        raise DeploymentError("A publish file exceeds the supported 64 MiB limit.")
    files = {name: (site / name).read_bytes() for name in sorted(expected)}
    validate_hashes(files)
    if files[".nojekyll"] or not files["emu/jam-emu.wasm"].startswith(b"\x00asm\x01\x00\x00\x00"):
        raise DeploymentError("Invalid .nojekyll marker or WebAssembly header.")
    if b"jambox-source.zip" not in files["emu/index.html"] or b"jambox-source.zip" not in files["LICENSES.html"]:
        raise DeploymentError("Published emulator and license page must link corresponding source.")
    if b"jambox-source.zip" not in files["device/index.html"]:
        raise DeploymentError("Published device check must link corresponding source.")
    validate_source(files["jambox-source.zip"], files)
    if with_trial:
        if b"jamboxfm-source.zip" not in files["install/index.html"]:
            raise DeploymentError("Published experimental installer must link corresponding firmware source.")
        if any(name.encode() not in files["LICENSES.html"] for name in (
                "firmware/licenses/AC79-SDK-LICENSE.txt", "firmware/licenses/AC79-SDK-README.md", "firmware/jamboxfm-source.zip")):
            raise DeploymentError("Published licenses page must link device SDK notices and corresponding source.")
        validate_trial(files)
    return files


def git(directory: Path, *arguments: str, input_text: str | None = None, check: bool = True) -> subprocess.CompletedProcess:
    # Do not inherit a caller's repository/index location. Keep normal Git
    # proxy/authentication configuration and credential helpers intact.
    environment = {key: value for key, value in os.environ.items() if key not in GIT_LOCATION_ENV}
    result = subprocess.run(["git", *GIT_OPTIONS, *arguments], cwd=directory, input=input_text,
                            env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode:
        raise DeploymentError(f"Git {arguments[0]} failed: {result.stderr.strip() or result.stdout.strip()}")
    return result


def remote_tip(directory: Path) -> str | None:
    lines = git(directory, "ls-remote", "--heads", "origin", BRANCH).stdout.splitlines()
    if not lines:
        return None
    if len(lines) != 1:
        raise DeploymentError("Remote returned more than one gh-pages reference.")
    match = re.fullmatch(r"([0-9a-f]{40,64})\s+refs/heads/gh-pages", lines[0])
    if not match:
        raise DeploymentError("Remote gh-pages reference was malformed.")
    return match[1]


def author_identity(root: Path, name: str | None, email: str | None) -> tuple[str, str]:
    name = name or git(root, "config", "--get", "user.name", check=False).stdout.strip()
    email = email or git(root, "config", "--get", "user.email", check=False).stdout.strip()
    if not name or not email or any(char in name + email for char in "\r\n\x00<>"):
        raise DeploymentError("Configure Git user.name and user.email, or pass --author-name and --author-email.")
    return name, email


@dataclass
class PreparedDeployment:
    directory: Path
    parent: str | None
    commit: str
    tree: str
    changed: bool
    files: dict[str, bytes]

    def finish(self, publish: bool = False) -> dict:
        # Also detect a race for no-op deployments, which otherwise make no push.
        if remote_tip(self.directory) != self.parent:
            raise DeploymentError("gh-pages changed during preparation; rerun against the new branch tip.")
        if self.changed:
            options = [] if publish else ["--dry-run"]
            # Explicit single-branch refspec; never force or alter a source branch.
            git(self.directory, "push", "--porcelain", *options, "origin", f"HEAD:{BRANCH}")
        return {
            "action": "unchanged" if not self.changed else "published" if publish else "dry-run",
            "branch": BRANCH, "parent": self.parent, "commit": self.commit, "tree": self.tree,
            "files": len(self.files), "bytes": sum(map(len, self.files.values())),
            "package_sha256": sha(self.files["SHA256SUMS"]),
        }


@contextmanager
def prepare_deployment(files: dict[str, bytes], remote: str, identity: tuple[str, str],
                       scratch: Path | None = None) -> Iterator[PreparedDeployment]:
    """Stage only the validated snapshot; the internal remote parameter supports local tests.

    The public CLI has no remote/site/branch override and always uses DESTINATION.
    Remote history is fetched as Git objects, never checked out as executable files.
    """
    if set(files) not in (EXPECTED_FILES, TRIAL_FILES):
        raise DeploymentError("Refusing to stage anything other than the fixed packaged file set.")
    if scratch:
        scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="jambox-pages-", dir=scratch) as temporary:
        directory = Path(temporary)
        git(directory, "init", "--quiet", "--template=", "--initial-branch=gh-pages")
        git(directory, "remote", "add", "origin", remote)
        git(directory, "config", "user.name", identity[0])
        git(directory, "config", "user.email", identity[1])
        parent = remote_tip(directory)
        if parent:
            git(directory, "fetch", "--quiet", "--no-tags", "origin", BRANCH)
            fetched = git(directory, "rev-parse", "FETCH_HEAD").stdout.strip()
            if fetched != parent:
                raise DeploymentError("gh-pages changed while fetching; rerun against the new branch tip.")
        for name, data in files.items():
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o644)
        git(directory, "add", "--", *sorted(files))
        tree = git(directory, "write-tree").stdout.strip()
        changed = not parent or git(directory, "rev-parse", f"{parent}^{{tree}}").stdout.strip() != tree
        if changed:
            parents = ["-p", parent] if parent else []
            feature = "experimental FM-1 installer" if "firmware/trial.json" in files else "device check"
            message = f"Publish JamBox browser instrument and {feature}\n\nPackage manifest SHA256: {sha(files['SHA256SUMS'])}\n"
            commit = git(directory, "-c", "commit.gpgsign=false", "commit-tree", tree, *parents,
                         input_text=message).stdout.strip()
        else:
            commit = parent
        git(directory, "update-ref", BRANCH, commit)
        yield PreparedDeployment(directory, parent, commit, tree, changed, files)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publish", action="store_true", help="push the validated site to gh-pages (default: Git push dry-run)")
    parser.add_argument("--author-name", help="deployment author if Git user.name is unavailable")
    parser.add_argument("--author-email", help="deployment author email if Git user.email is unavailable")
    args = parser.parse_args()
    try:
        files = validate_site(SITE)
        identity = author_identity(ROOT, args.author_name, args.author_email)
        scratch = ROOT.parent / "scratch"
        with prepare_deployment(files, DESTINATION, identity, scratch) as prepared:
            result = prepared.finish(args.publish)
        result["destination"] = DESTINATION
        print(json.dumps(result, indent=2, sort_keys=True))
    except (DeploymentError, OSError) as error:
        parser.exit(1, f"Deployment stopped: {error}\n")


if __name__ == "__main__":
    main()
