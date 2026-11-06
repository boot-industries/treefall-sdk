#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Static realtime-safety audit for Orpheus callback paths.

The audit is intentionally conservative and lexical. Every in-repository
audio-thread function listed in ``repository_target_rows`` receives the
hard-fail callback rules, including the blocking file-reader patterns. Adjacent
application targets keep the file-reader patterns as tracked debt. A lexical
scan cannot see forbidden work inside a callee that is not itself listed, so a
new helper reachable from a callback must be added to the table with its change.
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
    ".signal(": "audio callbacks must not signal events (juce::WaitableEvent::signal locks a mutex)",
    "WaitableEvent": "audio callbacks must not use WaitableEvent",
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
    # Blocking decoder/file access. In-repository callback paths must never
    # reach it; adjacent app targets still carry these as tracked debt below.
    "readSamples(": "audio callbacks must not perform blocking file reads",
    "->seek(": "audio callbacks must not seek file readers",
    "sf_readf_float": "audio callbacks must not call the blocking libsndfile decoder",
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


def _mask_comments_and_literals(lines: list[str]) -> list[str]:
    """Blank comment spans and literal contents, preserving length and line count."""
    masked: list[str] = []
    in_block = False
    quote = ""
    for line in lines:
        out: list[str] = []
        index = 0
        length = len(line)
        while index < length:
            char = line[index]
            nxt = line[index + 1] if index + 1 < length else ""
            if in_block:
                if char == "*" and nxt == "/":
                    out.append("  ")
                    index += 2
                    in_block = False
                else:
                    out.append(" ")
                    index += 1
            elif quote:
                if char == "\\" and nxt:
                    out.append("  ")
                    index += 2
                elif char == quote:
                    out.append(" ")
                    index += 1
                    quote = ""
                else:
                    out.append(" ")
                    index += 1
            elif char == "/" and nxt == "*":
                out.append("  ")
                index += 2
                in_block = True
            elif char == "/" and nxt == "/":
                out.extend(" " * (length - index))
                index = length
            elif char in ('"', "'"):
                quote = char
                out.append(" ")
                index += 1
            else:
                out.append(char)
                index += 1
        masked.append("".join(out))
    return masked


def scan_target(target: ScanTarget) -> list[Finding]:
    if not target.path.exists():
        if target.required:
            raise AuditConfigurationError(f"{target.label}: source file not found: {target.path}")
        return []

    text = target.path.read_text(encoding="utf-8", errors="replace")
    if target.function_regex:
        try:
            base_line, lines = extract_function(text, target.function_regex, target.label)
        except AuditConfigurationError:
            # An optional (adjacent-repo) target follows the sibling checkout:
            # a renamed or inlined function there is not a configuration error
            # of this repository, exactly as an absent file is not.
            if target.required:
                raise
            return []
    else:
        base_line, lines = 1, text.splitlines()
    masked = _mask_comments_and_literals(lines)

    findings: list[Finding] = []
    for pattern_items, hard_fail in target.resolved_pattern_sets():
        patterns = dict(pattern_items)
        for offset, (_original, timing_guarded) in enumerate(_timing_guarded_lines(lines)):
            line = masked[offset]
            if not line.strip():
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


def _method(owner: str, name: str) -> str:
    """Definition regex for a qualified member function ``owner::name``."""
    return rf"\b[\w:<>]+[\s*&]+{owner}::{name}\s*\("


def _free(name: str) -> str:
    """Definition regex for a free (typically anonymous-namespace) function."""
    return rf"\b[\w:<>]+\s+{name}\s*\("


@dataclass(frozen=True)
class TargetRow:
    """One source file and the audio-thread functions it must keep clean.

    ``names`` entries are either a bare name, resolved against ``owner`` (or as
    a free function when ``owner`` is None), or a ``(name, regex)`` pair for
    header-inline members whose definition does not carry ``owner::``.
    """

    label: str
    relpath: str
    owner: str | None
    names: tuple[str | tuple[str, str], ...]
    pattern_sets: tuple[PatternSet, ...] = (HARD_REALTIME_SET,)
    required: bool = True

    def expand(self, root: Path) -> list[ScanTarget]:
        expanded = []
        for entry in self.names:
            if isinstance(entry, tuple):
                name, regex = entry
            else:
                name = entry
                regex = _method(self.owner, name) if self.owner else _free(name)
            expanded.append(
                ScanTarget(
                    self.label.format(owner=self.owner, name=name),
                    root / self.relpath,
                    regex,
                    pattern_sets=self.pattern_sets,
                    required=self.required,
                )
            )
        return expanded


_TRANSPORT = "src/core/transport/transport_controller.cpp"
_ROUTING = "src/core/routing/routing_matrix.cpp"
_METER_STATE = "src/core/routing/routing_meter_state.cpp"
_CLIP_SOURCE = "src/core/transport/clip_source.cpp"
_COREAUDIO = "src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp"
_GAIN_SMOOTHER_H = "src/core/routing/gain_smoother.h"


def repository_target_rows() -> tuple[TargetRow, ...]:
    """Every audio-thread function in this repository, one table.

    A new function reachable from a render callback belongs here in the same
    change that adds it; the gate is lexical, so it cannot see a callee that is
    not listed.
    """
    return (
        # Driver callbacks.
        TargetRow("CoreAudio render callback", _COREAUDIO, "CoreAudioDriver", ("renderCallback",)),
        TargetRow(
            "WASAPI render loop",
            "src/platform/audio_drivers/wasapi/wasapi_driver.cpp",
            "WASAPIAudioDriver",
            ("audioLoop",),
        ),
        TargetRow(
            "CoreAudio callback helper {name}",
            _COREAUDIO,
            "CoreAudioDriver",
            (
                "recordInputRenderFailure",
                "recordInputFifoOverrun",
                "recordInputFifoUnderrun",
                "recordInputConversionFailure",
                "recordOutputConversionFailure",
                "failRealtimeConversion",
                "notifyInputOperation",
                "inputRenderCallback",
                "publishTerminalRouteOutcome",
            ),
        ),
        TargetRow(
            "CoreAudio route monitor callback helper {name}",
            "src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp",
            "CoreAudioRouteMonitor",
            ("closeAdmissionFromCallback",),
        ),
        # Transport render path and command consumer.
        TargetRow("Transport command consumer", _TRANSPORT, "TransportController", ("processCommands",)),
        TargetRow("Transport command detach helper", _TRANSPORT, "TransportController", ("detachPendingCommands",)),
        TargetRow("Transport command release helper", _TRANSPORT, "TransportController", ("releaseCommandNode",)),
        TargetRow("Transport render path", _TRANSPORT, "TransportController", ("processAudio",)),
        TargetRow(
            "Transport consumer helper {name}",
            _TRANSPORT,
            "TransportController",
            (
                "releasePendingStartReservations",
                "releasePendingSeekReservations",
                "publishStartSettlement",
                "startVoiceWithMode",
                "addActiveClip",
                "removeActiveVoice",
                "restartVoiceInPlace",
                "handlePlaybackBoundary",
                "allocateVoiceId",
                "allocateVoiceStartOrdinal",
                "incrementIngressCounter",
                "mergeAudioMeter",
                "markRoutingMetersUnmeasured",
                "rearmRoutingMeterWindow",
                "markLogicalGroupOutputUnmeasured",
                "collectRoutingMeters",
                "copyGroupOutputMeterRegion",
                "countActiveVoices",
                "findOldestVoice",
                "configureVoiceRouting",
                "calculateFadeGain",
                "applyDownmixLeft",
                "applyDownmixRight",
                "getCurrentPosition",
                "positionAtSamples",
            ),
        ),
        TargetRow(
            "Transport consumer free helper {name}",
            _TRANSPORT,
            None,
            (
                "incrementSaturated",
                "isLaterStartOrdinal",
                "beginOperatorStop",
                "playbackWindowStart",
                "playbackWindowEnd",
                "resetSegmentCursor",
                "advancePlaybackWindow",
                "clipDspProgramActive",
            ),
        ),
        TargetRow(
            "Transport callback publisher {name}",
            _TRANSPORT,
            "TransportController",
            ("postTransportEvent", "publishVoiceSnapshot", "publishCallbackTelemetry"),
        ),
        # ORP273 F-15: the registered-source lifetime helpers live in the
        # clip-source registry; the same realtime rules apply to them.
        TargetRow(
            "Transport consumer helper {name}",
            "src/core/transport/clip_source_registry.cpp",
            "ClipSourceRegistry",
            ("releaseSourceCommand", "retainActiveSource", "releaseActiveSource"),
        ),
        # Per-clip DSP and streaming clip-source callback paths.
        TargetRow("Clip DSP callback {name}", "src/core/transport/clip_dsp.cpp", "ClipDspProcessor", ("processFrame",)),
        TargetRow(
            "Streaming consumer lease helper {name}",
            _CLIP_SOURCE,
            "StreamingClipSource",
            ("releaseCommandPrime", "commitLoopAnchorTransition", "releaseReadyPin"),
        ),
        TargetRow(
            "Streaming callback helper {name}",
            _CLIP_SOURCE,
            "StreamingClipSource",
            (
                "copyFromResidentPage",
                "acquireTransientReadPin",
                "beginVoiceDemands",
                "addVoiceDemand",
                "refreshVoicePagePins",
                "publishVoiceDemands",
                "voiceWindowPages",
                "keepsPage",
            ),
        ),
        TargetRow("Clip source callback read {owner}::{name}", _CLIP_SOURCE, "PreparedClipSource", ("read",)),
        TargetRow(
            "Clip source callback read {owner}::{name}",
            _CLIP_SOURCE,
            "StreamingClipSource",
            ("read", "readCached", "readForVoice"),
        ),
        # ORP-SUITE-20261103-001: functions the render callback calls on every block.
        TargetRow(
            "Realtime telemetry {name}",
            "src/core/common/realtime_telemetry.cpp",
            "RealtimeTelemetry",
            ("beginRealtimeBlock", "reportUnderrunFromRealtime", "publishFromRealtime"),
        ),
        # Routing.
        TargetRow(
            "Routing render path",
            _ROUTING,
            "RoutingMatrix",
            ("processRouting",),
        ),
        TargetRow("Routing block render path", _ROUTING, "RoutingMatrix", ("processRoutingBlock",)),
        TargetRow(
            "Routing callback helper {name}",
            _ROUTING,
            "RoutingMatrix",
            (
                "getConfig",
                "refreshRenderGroupControls",
                "getGroupMeter",
                "getOutputMeter",
                "getMasterMeter",
                "copyGroupOutputMeterSnapshot",
                "resetLogicalGroupTruePeakHistories",
                "processStereoMetering",
                "publishChannelMeterSilence",
                "detectClipping",
                "dbToLinear",
                "activeChannelCount",
                "getHeadroomCompensation",
                ("fillRamp", r"\bvoid\s+fillRamp\s*\("),
            ),
        ),
        TargetRow(
            "Routing meter-state helper {name}",
            _METER_STATE,
            "RoutingMeterState",
            (
                "publishMasterPeak",
                "publishMasterRms",
                "incrementMasterClip",
                "publishOutputPeak",
                "publishOutputRms",
                "incrementOutputClip",
                "masterMeter",
                "outputMeter",
                "beginGroupOutputPublication",
                "publishGroupOutputHeader",
                "endGroupOutputPublication",
                "markGroupOutputUnmeasured",
                "groupOutputReadBegin",
                "groupOutputReadEnd",
                "groupOutputGroupCount",
                "groupOutputTopologyRevision",
                "groupOutputAvailability",
                "groupOutputCoherent",
                "linearToDb",
            ),
        ),
        TargetRow("Gain smoother {name}", "src/core/routing/gain_smoother.cpp", "GainSmoother", ("setTarget",)),
        TargetRow(
            "Gain smoother inline {name}",
            _GAIN_SMOOTHER_H,
            None,
            (
                ("process", r"\bfloat\s+process\s*\(\s*\)\s*(?=\{)"),
                ("beginBlock", r"\bRampState\s+beginBlock\s*\(\s*\)\s*(?=\{)"),
                ("next", r"\bfloat\s+next\s*\(\s*RampState&\s*\w+\s*\)\s*(?=\{)"),
                ("endBlock", r"\bvoid\s+endBlock\s*\(\s*const\s+RampState&\s*\w+\s*\)\s*(?=\{)"),
                ("adoptPendingTarget", r"\bvoid\s+adoptPendingTarget\s*\(\s*\)\s*noexcept\s*(?=\{)"),
            ),
        ),
        TargetRow(
            "True-peak meter inline {name}",
            "src/core/routing/true_peak_meter.h",
            None,
            (("process", r"\bfloat\s+process\s*\(\s*float\s+\w+\s*\)\s*(?=\{)"),),
        ),
        TargetRow(
            "Seqlock fence inline {name}",
            "src/core/common/seqlock_fence.h",
            None,
            (
                ("seqlockReleaseFence", r"\bvoid\s+seqlockReleaseFence\s*\(\s*\)\s*noexcept\s*(?=\{)"),
                ("seqlockAcquireFence", r"\bvoid\s+seqlockAcquireFence\s*\(\s*\)\s*noexcept\s*(?=\{)"),
            ),
        ),
        TargetRow("Audio input ring write", "src/core/audio_io/audio_input.cpp", "AudioInputRing", ("write",)),
        # Tier 2: callback-safe public classes hosts may call from their own callbacks.
        TargetRow(
            "Callback-safe ScrubResampler::{name}",
            "src/core/audio_io/scrub_resampler.cpp",
            "ScrubResampler",
            ("push_forward", "sample_at", "render", "processVariable", "forward_refill_hint"),
        ),
        TargetRow(
            "Callback-safe TriggerVoice::{name}",
            "src/core/audio_io/trigger_voice.cpp",
            "TriggerVoice",
            ("trigger", "applyTrigger", "render"),
        ),
        TargetRow(
            "Callback-safe PreparedTapeVarispeed::{name}",
            "src/core/audio_io/tape_varispeed.cpp",
            "PreparedTapeVarispeed",
            ("process", "drain"),
        ),
    )


def default_targets(root: Path, include_adjacent: bool) -> list[ScanTarget]:
    targets: list[ScanTarget] = []
    for row in repository_target_rows():
        targets.extend(row.expand(root))

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
        targets.extend(
            ScanTarget(
                f"FreqFinder analysis hand-off {function}",
                dev / "freqfinder/src/audio/AnalysisEngine.cpp",
                rf"(?:void|bool)\s+AnalysisEngine::{function}\s*\(",
                required=False,
                pattern_sets=(HARD_REALTIME_SET,),
            )
            for function in ("submitBlock", "enqueueJob", "populateJob")
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
