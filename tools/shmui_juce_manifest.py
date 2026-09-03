#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Synchronize and verify the vendored ShmUI-JUCE import manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "packages" / "shmui-juce"
MANIFEST = PACKAGE / "shmui-juce-import.json"
HASH_ALGORITHM = "sha256-path-nul-content-v1"
EXPECTED_TARGETS = {"Orpheus::shmui_juce", "Orpheus::shmui_juce_gl"}


def imported_files() -> list[pathlib.Path]:
    return sorted(
        (
            path
            for path in PACKAGE.rglob("*")
            if path.is_file() and path != MANIFEST and not path.name.startswith(".")
        ),
        key=lambda path: path.relative_to(PACKAGE).as_posix(),
    )


def canonical_content(path: pathlib.Path) -> bytes:
    """Hash imported text consistently across Git checkout platforms."""
    return path.read_bytes().replace(b"\r\n", b"\n")


def content_hash() -> str:
    digest = hashlib.sha256()
    for path in imported_files():
        relative = path.relative_to(PACKAGE).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        digest.update(canonical_content(path))
        digest.update(b"\0")
    return digest.hexdigest()


def load_manifest() -> dict[str, Any]:
    value = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError("manifest root must be an object")
    return value


def structural_failures(manifest: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    if manifest.get("schema_version") != 1:
        failures.append("schema_version must be 1")

    source = manifest.get("source")
    if not isinstance(source, dict):
        failures.append("source must be an object")
        source = {}
    revision = source.get("revision")
    if not isinstance(revision, str) or re.fullmatch(r"[0-9a-f]{40}", revision) is None:
        failures.append("source.revision must be a full lowercase Git SHA")
    if source.get("content_hash_algorithm") != HASH_ALGORITHM:
        failures.append(f"source.content_hash_algorithm must be {HASH_ALGORITHM}")
    if not isinstance(manifest.get("design_token_contract_version"), str):
        failures.append("design_token_contract_version must be a string")

    targets = manifest.get("targets")
    if not isinstance(targets, dict) or set(targets) != EXPECTED_TARGETS:
        failures.append("targets must declare exactly the core and OpenGL ShmUI targets")
        return failures

    core = targets["Orpheus::shmui_juce"]
    gl = targets["Orpheus::shmui_juce_gl"]
    if not isinstance(core.get("components"), list) or not core["components"]:
        failures.append("core target must list exported components")
    core_modules = core.get("required_juce_modules")
    if not isinstance(core_modules, list) or not core_modules:
        failures.append("core target must list required JUCE modules")
    elif "juce_opengl" in core_modules:
        failures.append("core target must not require juce_opengl")
    if gl.get("feature_flag") != "SHMUI_JUCE_ENABLE_OPENGL":
        failures.append("OpenGL target feature_flag is incorrect")
    if gl.get("default_enabled") is not False:
        failures.append("OpenGL target must default to disabled")
    if gl.get("required_juce_modules") != ["juce_opengl"]:
        failures.append("OpenGL target must declare only its incremental juce_opengl requirement")
    return failures


def check() -> int:
    manifest = load_manifest()
    failures = structural_failures(manifest)
    source = manifest.get("source", {})
    expected_hash = content_hash()
    if source.get("content_sha256") != expected_hash:
        failures.append("source.content_sha256 does not match imported package content")

    if failures:
        for failure in failures:
            print(f"ShmUI-JUCE manifest error: {failure}", file=sys.stderr)
        print("run: python3 tools/shmui_juce_manifest.py --sync", file=sys.stderr)
        return 1

    print(
        "ShmUI-JUCE manifest is consistent: "
        f"{len(imported_files())} files, sha256 {expected_hash}"
    )
    return 0


def sync() -> int:
    manifest = load_manifest()
    source = manifest.setdefault("source", {})
    source["content_hash_algorithm"] = HASH_ALGORITHM
    source["content_sha256"] = content_hash()
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sync", action="store_true")
    args = parser.parse_args()
    return check() if args.check else sync()


if __name__ == "__main__":
    raise SystemExit(main())
