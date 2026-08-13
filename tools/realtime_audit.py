#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Static realtime-safety audit for Orpheus callback paths.

The audit is intentionally conservative. Every in-repo realtime producer
receives the hard-fail callback rules. The transport render path additionally
receives the historical file-reader debt rules so one target can enforce both
contracts without choosing between them.
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
    "std::this_thread::yield": "audio callbacks must not yield",
    ".wait(": "audio callbacks must not wait",
    ".notify_one(": "audio callbacks must not notify waiters",
    ".notify_all(": "audio callbacks must not notify waiters",
    "std::condition_variable": "audio callbacks must not use condition variables",
    "std::lock_guard": "audio callbacks must not take locks",
    "std::unique_lock": "audio callbacks must not take locks",
    "std::scoped_lock": "audio callbacks must not take locks",
    "std::mutex": "audio callbacks must not touch mutexes",
    "std::shared_mutex": "audio callbacks must not touch mutexes",
    "fopen": "audio callbacks must not perform file I/O",
    "fread": "audio callbacks must not perform file I/O",
    "fwrite": "audio callbacks must not perform file I/O",
    "fprintf": "audio callbacks must not log to files/stderr",
    "printf(": "audio callbacks must not log",
    "std::cerr": "audio callbacks must not log",
    "std::cout": "audio callbacks must not log",
    "std::fstream": "audio callbacks must not perform file I/O",
    "std::ifstream": "audio callbacks must not perform file I/O",
    "std::ofstream": "audio callbacks must not perform file I/O",
    "std::function": "audio callbacks must not create/destroy std::function work",
    " new ": "audio callbacks must not allocate",
    "new (": "audio callbacks must not allocate",
    " delete ": "audio callbacks must not deallocate",
    "delete[]": "audio callbacks must not deallocate",
    "malloc(": "audio callbacks must not allocate",
    "calloc(": "audio callbacks must not allocate",
    "realloc(": "audio callbacks must not allocate",
    "free(": "audio callbacks must not deallocate",
    "make_unique": "audio callbacks must not allocate",
    "make_shared": "audio callbacks must not allocate",
    ".resize(": "audio callbacks must not grow containers",
    ".reserve(": "audio callbacks must not grow containers",
    ".push_back(": "audio callbacks must not grow containers",
    ".emplace_back(": "audio callbacks must not grow containers",
    ".insert(": "audio callbacks must not grow containers",
    ".assign(": "audio callbacks must not grow containers",
}

KNOWN_DEBT_PATTERNS = {
    "readSamples(": "file-backed readers still run from transport render path",
    "->seek(": "reader seeks can still occur from transport render commands",
    "sf_readf_float": "libsndfile is blocking decoder I/O",
    "juce::AudioBuffer<float> tempBuffer": "Clip Composer wraps/analyzes output in callback",
    "AudioAnalyzer::processBlock": "app-level visualization analysis runs in callback",
}

# A pattern set is explicit: (patterns, hard-fail). A target may carry more
# than one set, which is required for transport's hard rules + file-reader debt.
PatternSet = tuple[tuple[tuple[str, str], ...], bool]
HARD_REALTIME_SET: PatternSet = (tuple(FORBIDDEN_DRIVER_PATTERNS.items()), True)
FILE_READER_DEBT_SET: PatternSet = (tuple(KNOWN_DEBT_PATTERNS.items()), False)


@dataclass(frozen=True)
class ScanTarget:
    label: str
    path: Path
    function_regex: str | None
    # Retained for source compatibility with callers of the original scanner.
    hard_fail: bool | None = None
    pattern_sets: tuple[PatternSet, ...] = ()
    required: bool = True

    def resolved_pattern_sets(self) -> tuple[PatternSet, ...]:
        if self.pattern_sets:
            return self.pattern_sets
        return (HARD_REALTIME_SET if self.hard_fail else FILE_READER_DEBT_SET,)


class AuditConfigurationError(RuntimeError):
    """Raised when a required audit target cannot be located."""


@dataclass
class Finding:
    target: str
    path: Path
    line: int
    pattern: str
    message: str
    hard_fail: bool


def extract_function(text: str, function_regex: str, target_label: str) -> tuple[int, list[str]]:
    match = re.search(function_regex, text)
    if not match:
        raise AuditConfigurationError(f"{target_label}: function target not found: {function_regex}")

    start = match.start()
    brace = text.find("{", match.end())
    if brace == -1:
        raise AuditConfigurationError(f"{target_label}: function target has no body")

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


def _timing_guarded_lines(lines: list[str]) -> list[tuple[str, bool]]:
    """Mark lines inside the explicit callback-timing preprocessor guard."""
    guarded = False
    result: list[tuple[str, bool]] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("#if") and "ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING" in stripped:
            guarded = True
            result.append((line, True))
            continue
        result.append((line, guarded))
        if guarded and stripped.startswith("#endif"):
            guarded = False
    return result


def scan_target(target: ScanTarget) -> list[Finding]:
    if not target.path.exists():
        if target.required:
            raise AuditConfigurationError(f"{target.label}: source file not found: {target.path}")
        return []

    text = target.path.read_text(encoding="utf-8", errors="replace")
    if target.function_regex:
        base_line, lines = extract_function(text, target.function_regex, target.label)
    else:
        base_line, lines = 1, text.splitlines()

    findings: list[Finding] = []
    for pattern_items, hard_fail in target.resolved_pattern_sets():
        patterns = dict(pattern_items)
        for offset, (line, timing_guarded) in enumerate(_timing_guarded_lines(lines)):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            for pattern, message in patterns.items():
                if pattern == "std::chrono" and timing_guarded:
                    continue
                if pattern in line:
                    findings.append(
                        Finding(
                            target=target.label,
                            path=target.path,
                            line=base_line + offset,
                            pattern=pattern,
                            message=message,
                            hard_fail=hard_fail,
                        )
                    )
    return findings


def default_targets(root: Path, include_adjacent: bool) -> list[ScanTarget]:
    coreaudio_driver = root / "src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp"
    converter = root / "src/core/audio_io/directional_sample_rate_converter.cpp"
    targets = [
        ScanTarget(
            "CoreAudio render callback",
            coreaudio_driver,
            r"OSStatus\s+CoreAudioDriver::renderCallback\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "CoreAudio input capture callback",
            coreaudio_driver,
            r"OSStatus\s+CoreAudioDriver::inputRenderCallback\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "DirectionalSampleRateConverter push transfer",
            converter,
            r"DirectionalSrcTransfer\s+DirectionalSampleRateConverter::pushInput\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "DirectionalSampleRateConverter render transfer",
            converter,
            r"DirectionalSrcTransfer\s+DirectionalSampleRateConverter::renderOutput\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "WASAPI render loop",
            root / "src/platform/audio_drivers/wasapi/wasapi_driver.cpp",
            r"void\s+WASAPIAudioDriver::audioLoop\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "Transport render path",
            root / "src/core/transport/transport_controller.cpp",
            r"void\s+TransportController::processAudio\s*\(",
            pattern_sets=(HARD_REALTIME_SET, FILE_READER_DEBT_SET),
        ),
        ScanTarget(
            "Routing render path",
            root / "src/core/routing/routing_matrix.cpp",
            r"SessionGraphError\s+RoutingMatrix::processRouting\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "Routing block render path",
            root / "src/core/routing/routing_matrix.cpp",
            r"SessionGraphError\s+RoutingMatrix::processRoutingBlock\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
        ),
        ScanTarget(
            "Audio input ring write",
            root / "src/core/audio_io/audio_input.cpp",
            r"size_t\s+AudioInputRing::write\s*\(",
            pattern_sets=(HARD_REALTIME_SET,),
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
                    required=False,
                    pattern_sets=(FILE_READER_DEBT_SET,),
                ),
                ScanTarget(
                    "FourTrack engine callback",
                    dev / "fourtrack/src/fourtrack/engine/engine.cpp",
                    r"void\s+Engine::processAudio\s*\(",
                    required=False,
                    pattern_sets=(FILE_READER_DEBT_SET,),
                ),
                ScanTarget(
                    "FreqFinder processBlock",
                    dev / "freqfinder/src/PluginProcessor.cpp",
                    r"void\s+FreqFinderAudioProcessor::processBlock\s*\(",
                    required=False,
                    pattern_sets=(FILE_READER_DEBT_SET,),
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
    try:
        for target in default_targets(root, args.include_adjacent):
            findings.extend(scan_target(target))
    except AuditConfigurationError as error:
        print(f"Realtime audit configuration error: {error}", file=sys.stderr)
        return 2

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
