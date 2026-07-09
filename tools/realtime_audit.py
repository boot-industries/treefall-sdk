#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Static realtime-safety audit for Orpheus callback paths.

The audit is intentionally conservative. It fails on forbidden patterns in
hardware-driver callbacks and reports known transport/app debt separately so CI
can guard new driver regressions while the streaming architecture is developed.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


FORBIDDEN_DRIVER_PATTERNS = {
    "std::chrono": "callback-local timing is opt-in diagnostics only",
    "sleep_for": "audio callbacks must never sleep",
    "std::this_thread::sleep": "audio callbacks must never sleep",
    "std::lock_guard": "audio callbacks must not take locks",
    "std::unique_lock": "audio callbacks must not take locks",
    "std::mutex": "audio callbacks must not touch mutexes",
    "fopen": "audio callbacks must not perform file I/O",
    "fprintf": "audio callbacks must not log to files/stderr",
    "std::function": "audio callbacks must not create/destroy std::function work",
    " new ": "audio callbacks must not allocate",
    "make_unique": "audio callbacks must not allocate",
    "make_shared": "audio callbacks must not allocate",
    ".resize(": "audio callbacks must not grow containers",
    ".push_back(": "audio callbacks must not grow containers",
    ".emplace_back(": "audio callbacks must not grow containers",
}

KNOWN_DEBT_PATTERNS = {
    "readSamples(": "file-backed readers still run from transport render path",
    "->seek(": "reader seeks can still occur from transport render commands",
    "sf_readf_float": "libsndfile is blocking decoder I/O",
    "juce::AudioBuffer<float> tempBuffer": "Clip Composer wraps/analyzes output in callback",
    "AudioAnalyzer::processBlock": "app-level visualization analysis runs in callback",
}


@dataclass(frozen=True)
class ScanTarget:
    label: str
    path: Path
    function_regex: str | None
    hard_fail: bool


@dataclass
class Finding:
    target: str
    path: Path
    line: int
    pattern: str
    message: str
    hard_fail: bool


def extract_function(text: str, function_regex: str) -> tuple[int, list[str]]:
    match = re.search(function_regex, text)
    if not match:
        return 1, []

    start = match.start()
    brace = text.find("{", match.end())
    if brace == -1:
        return text[:start].count("\n") + 1, []

    depth = 0
    end = brace
    for idx in range(brace, len(text)):
        char = text[idx]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                end = idx + 1
                break

    base_line = text[:start].count("\n") + 1
    return base_line, text[start:end].splitlines()


def scan_target(target: ScanTarget) -> list[Finding]:
    if not target.path.exists():
        return []

    text = target.path.read_text(encoding="utf-8", errors="replace")
    if target.function_regex:
        base_line, lines = extract_function(text, target.function_regex)
    else:
        base_line, lines = 1, text.splitlines()

    patterns = FORBIDDEN_DRIVER_PATTERNS if target.hard_fail else KNOWN_DEBT_PATTERNS
    findings: list[Finding] = []
    for offset, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        for pattern, message in patterns.items():
            if pattern in line:
                findings.append(
                    Finding(
                        target=target.label,
                        path=target.path,
                        line=base_line + offset,
                        pattern=pattern,
                        message=message,
                        hard_fail=target.hard_fail,
                    )
                )
    return findings


def default_targets(root: Path, include_adjacent: bool) -> list[ScanTarget]:
    targets = [
        ScanTarget(
            "CoreAudio render callback",
            root / "src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp",
            r"OSStatus\s+CoreAudioDriver::renderCallback\s*\(",
            True,
        ),
        ScanTarget(
            "Transport render path",
            root / "src/core/transport/transport_controller.cpp",
            r"void\s+TransportController::processAudio\s*\(",
            False,
        ),
        ScanTarget(
            "libsndfile reader",
            root / "src/core/audio_io/audio_file_reader_libsndfile.cpp",
            r"Result<size_t>\s+AudioFileReaderLibsndfile::readSamples\s*\(",
            False,
        ),
    ]

    if include_adjacent:
        dev = root.parent
        targets.extend(
            [
                ScanTarget(
                    "Clip Composer AudioEngine callback",
                    dev / "clip-composer/Source/Audio/AudioEngine.cpp",
                    r"void\s+AudioEngine::processAudio\s*\(",
                    False,
                ),
                ScanTarget(
                    "FourTrack engine callback",
                    dev / "fourtrack/src/fourtrack/engine/engine.cpp",
                    r"void\s+Engine::processAudio\s*\(",
                    False,
                ),
                ScanTarget(
                    "FreqFinder processBlock",
                    dev / "freqfinder/src/PluginProcessor.cpp",
                    r"void\s+FreqFinderAudioProcessor::processBlock\s*\(",
                    False,
                ),
            ]
        )
    return targets


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--include-adjacent", action="store_true")
    parser.add_argument("--fail-known-debt", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    findings: list[Finding] = []
    for target in default_targets(root, args.include_adjacent):
      findings.extend(scan_target(target))

    hard = [finding for finding in findings if finding.hard_fail]
    debt = [finding for finding in findings if not finding.hard_fail]

    for finding in findings:
        level = "FAIL" if finding.hard_fail else "DEBT"
        rel = finding.path if not finding.path.is_relative_to(root) else finding.path.relative_to(root)
        print(
            f"{level}: {finding.target}: {rel}:{finding.line}: "
            f"{finding.pattern} ({finding.message})"
        )

    if hard or (args.fail_known_debt and debt):
        return 1

    print(
        f"Realtime audit passed: {len(hard)} hard failures, "
        f"{len(debt)} tracked debt findings."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
