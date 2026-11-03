#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Deterministic unit fixtures for the repository realtime scanner."""

from __future__ import annotations
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("realtime_audit", ROOT / "tools" / "realtime_audit.py")
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUDIT
SPEC.loader.exec_module(AUDIT)


class RealtimeAuditFixturesTest(unittest.TestCase):
    def scan(self, source: str, *, pattern_sets=None):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.cpp"
            path.write_text(source, encoding="utf-8")
            target = AUDIT.ScanTarget(
                "fixture",
                path,
                r"void\s+render\s*\(",
                pattern_sets=pattern_sets or (AUDIT.HARD_REALTIME_SET,),
            )
            return AUDIT.scan_target(target)

    def test_guarded_timing_is_allowed_but_unguarded_timing_fails(self):
        guarded = """
        void render() {
        #if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
          std::chrono::steady_clock::now();
        #endif
        }
        """
        self.assertEqual(self.scan(guarded), [])

        unguarded = """
        void render() {
          std::chrono::steady_clock::now();
        }
        """
        findings = self.scan(unguarded)
        self.assertEqual([finding.pattern for finding in findings], ["std::chrono"])
        self.assertTrue(findings[0].hard_fail)

    def test_forbidden_realtime_families_are_all_hard_failures(self):
        body = "\n".join(
            [
                "void render() {",
                "  std::mutex mutex; std::lock_guard lock(mutex);",
                "  std::condition_variable condition; condition.wait(lock);",
                "  condition.notify_one(); condition.notify_all();",
                "  std::this_thread::sleep_for(duration);",
                "  auto* value = new int; delete value;",
                "  auto* bytes = malloc(1); bytes = realloc(bytes, 2); free(bytes);",
                "  std::make_unique<int>(); std::make_shared<int>();",
                "  values.resize(2); values.reserve(2); values.push_back(1);",
                "  values.emplace_back(1); values.insert(values.begin(), 1);",
                "  values.assign(1, 1);",
                "  fopen(\"x\", \"r\"); fread(nullptr, 1, 1, nullptr);",
                "  fprintf(stderr, \"x\"); std::function<void()> callback;",
                "}",
            ]
        )
        findings = self.scan(body)
        patterns = {finding.pattern for finding in findings}
        for expected in (
            "std::mutex",
            ".wait(",
            ".notify_one(",
            ".notify_all(",
            "sleep_for",
            " new ",
            " delete ",
            "malloc(",
            "realloc(",
            "free(",
            "make_unique",
            "make_shared",
            ".resize(",
            ".reserve(",
            ".push_back(",
            ".emplace_back(",
            ".insert(",
            ".assign(",
            "fopen",
            "fread",
            "fprintf",
            "std::function",
        ):
            self.assertIn(expected, patterns)
        self.assertTrue(findings)
        self.assertTrue(all(finding.hard_fail for finding in findings))

    def test_one_target_receives_hard_rules_and_file_reader_debt(self):
        source = """
        void render() {
          readSamples();
          std::mutex mutex;
        }
        """
        findings = self.scan(
            source,
            pattern_sets=(AUDIT.HARD_REALTIME_SET, AUDIT.FILE_READER_DEBT_SET),
        )
        self.assertEqual({finding.pattern for finding in findings}, {"readSamples(", "std::mutex"})
        self.assertEqual({finding.hard_fail for finding in findings}, {True, False})

    def test_event_signalling_is_a_hard_failure_in_a_callback(self):
        # juce::WaitableEvent::signal() takes the mutex the waiting worker
        # holds, so signalling from a render callback is a lock.
        findings = self.scan(
            """
            void render() {
              jobAvailableEvent_.signal();
            }
            """
        )
        self.assertEqual([finding.pattern for finding in findings], [".signal("])
        self.assertTrue(findings[0].hard_fail)
        self.assertFalse(self.scan("void render() { juce::Thread::signalThreadShouldExit(); }"))

    def test_adjacent_freqfinder_hand_off_is_scanned_with_hard_rules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "orpheus-sdk"
            engine = Path(directory) / "freqfinder" / "src" / "audio" / "AnalysisEngine.cpp"
            engine.parent.mkdir(parents=True)
            engine.write_text(
                """
                void AnalysisEngine::submitBlock(int n) { n += 1; }
                bool AnalysisEngine::enqueueJob(int n) {
                  jobAvailableEvent_.signal();
                  return n > 0;
                }
                void AnalysisEngine::populateJob(int n) { n += 1; }
                """,
                encoding="utf-8",
            )
            findings = []
            for target in AUDIT.default_targets(root, True):
                if target.path == engine:
                    findings.extend(AUDIT.scan_target(target))
            self.assertEqual([(f.target, f.pattern) for f in findings],
                             [("FreqFinder analysis hand-off enqueueJob", ".signal(")])
            self.assertTrue(findings[0].hard_fail)

    def test_default_adjacent_targets_are_opt_in(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertFalse(any("adjacent" in target.label.lower() for target in AUDIT.default_targets(root, False)))
            self.assertTrue(any("Clip Composer" in target.label for target in AUDIT.default_targets(root, True)))
    def test_missing_configured_function_fails_loudly(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.cpp"
            path.write_text("void unrelated() {}\n", encoding="utf-8")
            target = AUDIT.ScanTarget("missing callback", path, r"void\s+render\s*\(")
            with self.assertRaises(AUDIT.AuditConfigurationError):
                AUDIT.scan_target(target)

    def test_optional_adjacent_target_tolerates_a_renamed_function(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.cpp"
            path.write_text("void unrelated() {}\n", encoding="utf-8")
            optional = AUDIT.ScanTarget(
                "adjacent callback", path, r"void\s+render\s*\(", required=False
            )
            self.assertEqual(AUDIT.scan_target(optional), [])



if __name__ == "__main__":
    unittest.main()
