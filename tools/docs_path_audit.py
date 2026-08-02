#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Docs-path validation gate (ORP133 G6 / ORP136 §2.7).

Keeps the ORP133 documentation truth pass from rotting. Fails on:

1. Internal markdown links in LIVE docs that point at nonexistent paths.
2. References to ``apps/clip-composer`` — the app was extracted to the
   external ``chrislyons/clip-composer`` repository (ORP131). Historical
   mentions are allowed only when the same line clearly labels them as such
   (``former``/``archiv``/``extracted``/``ORP131``).
3. References to CMake options that do not exist in the root CMakeLists.txt
   (e.g. the removed ``ORPHEUS_ENABLE_APP_CLIP_COMPOSER``).

Scope: live top-level docs + docs/*.md. Frozen historical records (the ORP074
sprint records, the TypeScript-era upgrade/migration guides, docs/orp/,
docs/archive/) are exempt — they describe the repo as it was, and docs/INDEX.md
labels them as historical.
"""

from __future__ import annotations

import argparse
import re
import sys
import urllib.parse
from dataclasses import dataclass
from pathlib import Path

# Live documents whose internal links must resolve.
LINK_CHECKED_DOCS = [
    "README.md",
    "ARCHITECTURE.md",
    "ROADMAP.md",
    "CLAUDE.md",
]
LINK_CHECKED_GLOBS = ["docs/**/*.md"]

# Documents scanned for forbidden patterns (superset of the above).
PATTERN_CHECKED_EXTRA = ["CHANGELOG.md"]

# Frozen historical records: they intentionally describe a repo state that no
# longer exists (pre-extraction paths, TypeScript-era options). docs/INDEX.md
# labels them as historical. New live docs must NOT be added here casually.
HISTORICAL_DOCS = {
    "docs/SDK_SPRINT_SUMMARY.md",
    "docs/SDK_TEAM_HANDOFF.md",
    "docs/UPGRADING_TO_1.0.md",
    "docs/MIGRATION_v0_to_v1.md",
}

AUTHORITY_DOCS = {
    "docs/REALTIME_AUDIT.md",
    "docs/SUPPORT_MATRIX.md",
}
EXCLUDED_CMAKE_COMPONENTS = {".git", "_deps", "CMakeFiles"}
HISTORICAL_DOC_PREFIXES = ("docs/orp/", "docs/archive/", "docs/api/archive/", "docs/tmp/")
# Handles both [t](path) and the angle-bracket form [t](<path with (parens)>).
MD_LINK_RE = re.compile(r"\[[^\]]*\]\((<[^>]*>|[^)\s]+)\)")
EXTRACTED_APP_RE = re.compile(r"apps/clip-composer")
HISTORICAL_LABEL_RE = re.compile(r"former|archiv|extracted|ORP131", re.IGNORECASE)
# Only -D-prefixed tokens are treated as CMake configure options; bare
# ORPHEUS_* identifiers can legitimately be env vars or code symbols.
CMAKE_OPTION_RE = re.compile(r"-D(ORPHEUS_[A-Z0-9_]+|ORP_[A-Z0-9_]+)")
CMAKE_DEFINED_RE = re.compile(r"\b(ORPHEUS_[A-Z0-9_]+|ORP_[A-Z0-9_]+)\b")

# Non-option CMake/doc tokens that legitimately match the option regex.
CMAKE_TOKEN_ALLOWLIST = {
    "ORP_BUILD_REAPER",   # documented as deprecated in CMakeLists.txt itself
    "ORP_BUILD_MINHOST",  # documented as deprecated in CMakeLists.txt itself
    "ORPHEUS_BUILD_SHARED",  # compatibility shim handled in CMakeLists.txt
}


@dataclass
class Violation:
    path: Path
    line: int
    kind: str
    detail: str


def _is_excluded_cmake_path(path: Path, root: Path) -> bool:
    relative = path.relative_to(root)
    return any(
        component in EXCLUDED_CMAKE_COMPONENTS or component.startswith("build")
        for component in relative.parts
    )


def repo_cmake_files(root: Path) -> list[Path]:
    """Return source CMake files without generated/build authority."""
    files = {
        path
        for path in root.rglob("CMakeLists.txt")
        if not _is_excluded_cmake_path(path, root)
    }
    files.update(
        path
        for path in root.rglob("*.cmake")
        if not _is_excluded_cmake_path(path, root)
    )
    return sorted(files)


def repo_cmake_tokens(root: Path) -> set[str]:
    """All ORPHEUS_*/ORP_* identifiers defined or handled by the build."""
    tokens: set[str] = set(CMAKE_TOKEN_ALLOWLIST)
    for cmake in repo_cmake_files(root):
        tokens.update(CMAKE_DEFINED_RE.findall(cmake.read_text(encoding="utf-8")))
    return tokens


def iter_docs(root: Path) -> list[Path]:
    docs: list[Path] = []
    for name in LINK_CHECKED_DOCS:
        path = root / name
        if path.exists():
            docs.append(path)
    for pattern in LINK_CHECKED_GLOBS:
        docs.extend(sorted(root.glob(pattern)))
    return [
        doc
        for doc in docs
        if str(doc.relative_to(root)) not in HISTORICAL_DOCS
        and not str(doc.relative_to(root)).startswith(HISTORICAL_DOC_PREFIXES)
    ]


def check_links(doc: Path, root: Path) -> list[Violation]:
    violations: list[Violation] = []
    text = doc.read_text(encoding="utf-8", errors="replace")
    in_code_block = False
    for lineno, line in enumerate(text.splitlines(), start=1):
        if line.strip().startswith("```"):
            in_code_block = not in_code_block
            continue
        if in_code_block:
            continue
        for match in MD_LINK_RE.finditer(line):
            target = match.group(1).strip()
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1].strip()
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            if "://" in target:
                continue
            path_part = urllib.parse.unquote(target.split("#", 1)[0])
            if not path_part:
                continue
            if path_part.startswith("/"):
                resolved = root / path_part.lstrip("/")
            else:
                resolved = (doc.parent / path_part).resolve()
            if not resolved.exists():
                violations.append(
                    Violation(doc, lineno, "broken-link", f"link target does not exist: {target}")
                )
    return violations


def check_patterns(doc: Path, valid_tokens: set[str]) -> list[Violation]:
    violations: list[Violation] = []
    text = doc.read_text(encoding="utf-8", errors="replace")
    for lineno, line in enumerate(text.splitlines(), start=1):
        if EXTRACTED_APP_RE.search(line) and not HISTORICAL_LABEL_RE.search(line):
            violations.append(
                Violation(
                    doc,
                    lineno,
                    "extracted-app-path",
                    "apps/clip-composer is external (ORP131); label historical mentions "
                    "as former/archived or point at chrislyons/clip-composer",
                )
            )
        for token in CMAKE_OPTION_RE.findall(line):
            if token not in valid_tokens:
                violations.append(
                    Violation(
                        doc,
                        lineno,
                        "unknown-cmake-option",
                        f"{token} is not defined in CMakeLists.txt/cmake/ (removed or misspelled)",
                    )
                )
    return violations


def check_authority_documents(root: Path) -> list[Violation]:
    violations: list[Violation] = []
    for relative in sorted(AUTHORITY_DOCS):
        path = root / relative
        if not path.exists():
            violations.append(
                Violation(
                    root,
                    0,
                    "missing-authority",
                    f"required source-of-truth document does not exist: {relative}",
                )
            )
    return violations


def check_installed_header_contract(root: Path) -> list[Violation]:
    """Reject the contradictory any-thread getDeviceInfo() promise."""
    path = root / "include/orpheus/audio_driver_manager.h"
    if not path.exists():
        return [Violation(root, 0, "missing-header", "public manager header does not exist")]
    text = path.read_text(encoding="utf-8", errors="replace")
    start = text.find("Get detailed information about specific device")
    end = text.find("Set active audio device", start)
    segment = text[start:end if end != -1 else None]
    violations: list[Violation] = []
    if re.search(r"thread-safe.*any thread", segment, re.IGNORECASE | re.DOTALL):
        line = text[:start].count("\n") + 1 if start >= 0 else 1
        violations.append(
            Violation(
                path,
                line,
                "contradictory-thread-contract",
                "getDeviceInfo() must not claim thread-safe any-thread access",
            )
        )
    if "UI thread" not in segment and "control thread" not in segment:
        line = text[:start].count("\n") + 1 if start >= 0 else 1
        violations.append(
            Violation(
                path,
                line,
                "missing-thread-contract",
                "getDeviceInfo() must be classified as control/UI-thread-only",
            )
        )
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()

    valid_tokens = repo_cmake_tokens(root)
    violations: list[Violation] = check_authority_documents(root)
    violations.extend(check_installed_header_contract(root))

    docs = iter_docs(root)
    for doc in docs:
        violations.extend(check_links(doc, root))
        violations.extend(check_patterns(doc, valid_tokens))

    for name in PATTERN_CHECKED_EXTRA:
        path = root / name
        if path.exists():
            violations.extend(check_patterns(path, valid_tokens))

    for violation in violations:
        rel = violation.path.relative_to(root)
        print(f"FAIL: {rel}:{violation.line}: [{violation.kind}] {violation.detail}")

    if violations:
        print(f"Docs-path audit failed: {len(violations)} violation(s).")
        return 1

    print(f"Docs-path audit passed: {len(docs) + len(PATTERN_CHECKED_EXTRA)} documents checked.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
