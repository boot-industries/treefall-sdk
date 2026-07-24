#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Synchronize and verify public SDK version claims against CMake project()."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CMAKE_PROJECT = ROOT / "CMakeLists.txt"

VERSION_TARGETS: tuple[tuple[pathlib.Path, re.Pattern[str], str], ...] = (
    (
        ROOT / "README.md",
        re.compile(r"(?m)^(\*\*Current version:\*\* )\d+\.\d+\.\d+"),
        r"\g<1>{version}",
    ),
    (
        ROOT / "examples/README.md",
        re.compile(r"(?m)^(\*\*SDK Version:\*\* ).+$"),
        r"\g<1>{version}",
    ),
    (
        ROOT / "examples/simple_player/README.md",
        re.compile(r"(?m)^(\*\*SDK Version:\*\* ).+$"),
        r"\g<1>{version}",
    ),
    (
        ROOT / "examples/multi_clip_trigger/README.md",
        re.compile(r"(?m)^(\*\*SDK Version:\*\* ).+$"),
        r"\g<1>{version}",
    ),
    (
        ROOT / "examples/offline_renderer/README.md",
        re.compile(r"(?m)^(\*\*SDK Version:\*\* ).+$"),
        r"\g<1>{version}",
    ),
)


def project_version() -> str:
    text = CMAKE_PROJECT.read_text(encoding="utf-8")
    match = re.search(
        r"project\s*\(\s*orpheus\s+VERSION\s+(\d+\.\d+\.\d+)\b",
        text,
        flags=re.IGNORECASE,
    )
    if match is None:
        raise RuntimeError("CMake project(orpheus VERSION ...) was not found")
    return match.group(1)


def rendered_text(path: pathlib.Path, pattern: re.Pattern[str], replacement: str) -> str:
    original = path.read_text(encoding="utf-8")
    rendered, count = pattern.subn(replacement, original, count=1)
    if count != 1:
        raise RuntimeError(f"expected exactly one version field in {path.relative_to(ROOT)}")
    return rendered


def sync(version: str) -> int:
    for path, pattern, template in VERSION_TARGETS:
        replacement = template.format(version=version)
        rendered = rendered_text(path, pattern, replacement)
        path.write_text(rendered, encoding="utf-8")
    return 0


def check(version: str) -> int:
    failures: list[str] = []
    for path, pattern, template in VERSION_TARGETS:
        replacement = template.format(version=version)
        original = path.read_text(encoding="utf-8")
        rendered = rendered_text(path, pattern, replacement)
        if rendered != original:
            failures.append(str(path.relative_to(ROOT)))

    metadata_path = ROOT / "release/orpheus-sdk.json"
    if metadata_path.exists():
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if metadata.get("version") != version:
            failures.append(str(metadata_path.relative_to(ROOT)))

    if failures:
        print(f"SDK version is {version}; divergent claims: {', '.join(failures)}", file=sys.stderr)
        print("run: python3 tools/version_contract.py --sync", file=sys.stderr)
        return 1
    print(f"SDK version contract is consistent: {version}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sync", action="store_true")
    args = parser.parse_args()

    version = project_version()
    return check(version) if args.check else sync(version)


if __name__ == "__main__":
    raise SystemExit(main())
