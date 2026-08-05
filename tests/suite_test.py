#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Deterministic unit contracts for the Orpheus Suite coordinator."""

from __future__ import annotations

import copy
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("suite_tool", ROOT / "tools" / "suite.py")
assert SPEC and SPEC.loader
SUITE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = SUITE
SPEC.loader.exec_module(SUITE)


class SuiteToolTest(unittest.TestCase):
    @staticmethod
    def git(cwd: Path, *args: str) -> str:
        result = subprocess.run(
            ["git", *args],
            cwd=cwd,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        return result.stdout.strip()

    def make_repo(self, root: Path) -> tuple[Path, str, Path]:
        bare = root / "remote.git"
        repo = root / "repo"
        subprocess.run(["git", "init", "--bare", str(bare)], check=True, stdout=subprocess.PIPE)
        subprocess.run(["git", "init", "-b", "main", str(repo)], check=True, stdout=subprocess.PIPE)
        self.git(repo, "config", "user.name", "Suite Test")
        self.git(repo, "config", "user.email", "suite-test@example.invalid")
        (repo / "artifact.txt").write_text("stable\n", encoding="utf-8")
        self.git(repo, "add", "artifact.txt")
        self.git(repo, "commit", "-m", "test: seed suite fixture")
        revision = self.git(repo, "rev-parse", "HEAD")
        self.git(repo, "remote", "add", "origin", str(bare))
        self.git(repo, "push", "-u", "origin", "main")
        self.git(repo, "fetch", "origin", "main")
        return repo, revision, bare

    @staticmethod
    def manifest(workspace: Path, revision: str, remote: Path) -> dict:
        artifact_hash = SUITE.hash_file(workspace / "repo" / "artifact.txt")
        return {
            "$schema": "./schema/orpheus-suite.schema.json",
            "schema_version": 1,
            "suite_id": "test-suite",
            "suite_version": "0.1.0",
            "release_channel": "candidate",
            "manifest_revision": "2026-08-05",
            "workspace": {
                "root_environment": "ORPHEUS_SUITE_WORKSPACE",
                "default_root": ".",
                "paths_are_relative": True,
                "require_clean_worktrees_for_apply": True,
            },
            "channels": {
                "development": {"mode": "floating", "branch": "main", "snapshot_id": None},
                "candidate": {"mode": "pinned", "branch": "main", "snapshot_id": "candidate"},
                "stable": {"mode": "pinned", "branch": "main", "snapshot_id": None},
            },
            "coordination": {
                "system_of_record": "test",
                "change_id_format": "ORP-SUITE-YYYYMMDD-NNN",
                "branch_prefix": "suite/",
                "pr_title_prefix": "suite:",
                "merge_order": ["repo"],
                "pr_grouping": "one logical repository change per PR",
                "credentials": "test",
                "loop_prevention": "test",
            },
            "repositories": [
                {
                    "id": "repo",
                    "path": "repo",
                    "remote": {
                        "fetch_url": str(remote),
                        "fetch_remote": "origin",
                        "push_url": None,
                        "push_remote": None,
                    },
                    "default_branch": "main",
                    "role": "test",
                    "source_of_truth": ["artifact.txt"],
                    "generated_artifacts": [
                        {
                            "id": "test-artifact",
                            "path": "artifact.txt",
                            "hash_type": "sha256-file-v1",
                            "expected_sha256": artifact_hash,
                            "source_repository": "repo",
                            "source_revision": revision,
                            "current_source_revision": revision,
                            "source_paths": ["artifact.txt"],
                        }
                    ],
                    "verification": [],
                }
            ],
            "dependencies": [],
            "release_policy": {
                "order": ["repo"],
                "candidate": {
                    "requires": ["tests"],
                    "acceptance": [{"repository": "repo", "kind": "candidate-ui"}],
                    "publishes": "candidate",
                },
                "stable": {
                    "requires": ["tests"],
                    "acceptance": [{"repository": "repo", "kind": "stable-hardware"}],
                    "publishes": "stable",
                },
                "rollback": {"unit": "whole suite", "behavior": "guarded", "failure": "stop"},
            },
            "snapshots": {
                "candidate": {
                    "id": "candidate",
                    "suite_version": "0.1.0",
                    "channel": "candidate",
                    "state": "candidate",
                    "immutable": True,
                    "observed_at": "2026-08-05",
                    "repositories": {
                        "repo": {"commit": revision, "branch": "main", "tag": None}
                    },
                    "verification": {"status": "passed", "evidence": ["unit"]},
                    "human_acceptance": {
                        "status": "passed",
                        "records": [
                            {
                                "repository": "repo",
                                "kind": "candidate-ui",
                                "status": "passed",
                                "evidence": "candidate.txt",
                            }
                        ],
                    },
                }
            },
        }

    def test_github_ssh_and_https_urls_share_identity(self):
        self.assertEqual(
            SUITE.normalized_url("https://github.com/ChrisLyons/orpheus-sdk.git"),
            SUITE.normalized_url("git@github.com:chrislyons/orpheus-sdk.git"),
        )
        self.assertEqual(
            SUITE.normalized_url("ssh://git@github.com/chrislyons/orpheus-sdk"),
            "github.com/chrislyons/orpheus-sdk",
        )

    def test_environment_is_forwarded_without_replacing_process_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            result = SUITE.run_command(
                [sys.executable, "-c", "import os; print(os.getenv('SUITE_TEST_VALUE')); print(os.getenv('PATH') is not None)"],
                Path(directory),
                environment={"SUITE_TEST_VALUE": "isolated"},
            )
        self.assertTrue(result.ok)
        self.assertIn("isolated", result.output)
        self.assertIn("True", result.output)

    def test_snapshot_status_attaches_fresh_artifact_and_not_required_push(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, revision, remote = self.make_repo(root)
            manifest = self.manifest(root, revision, remote)
            status = SUITE.snapshot_status(manifest, root, None)
        item = status["repositories"][0]
        self.assertEqual(item["remote"]["push_status"], "not-required")
        self.assertEqual(item["artifacts"][0]["status"], "fresh")
        self.assertEqual(item["artifacts"][0]["provenance_status"], "source-current")
        self.assertEqual(item["reachability"]["status"], "reachable")
        self.assertEqual(SUITE.status_errors(status), [])

    def test_manifest_rejects_duplicate_or_unknown_acceptance_pairs(self):
        manifest = {
            "schema_version": 1,
            "suite_id": "suite",
            "suite_version": "0.1.0",
            "release_channel": "development",
            "workspace": {"paths_are_relative": True},
            "repositories": [{"id": "one", "path": "one", "remote": {"fetch_url": "x", "fetch_remote": "origin", "push_url": None, "push_remote": None}, "verification": []}],
            "dependencies": [],
            "release_policy": {
                "order": ["one"],
                "candidate": {"acceptance": [{"repository": "one", "kind": "gate"}, {"repository": "one", "kind": "gate"}]},
                "stable": {"acceptance": [{"repository": "missing", "kind": "gate"}]},
            },
            "snapshots": {},
        }
        errors = SUITE.validate_manifest(manifest)
        self.assertTrue(any("duplicates acceptance pair" in error for error in errors))
        self.assertTrue(any("repository is unknown" in error for error in errors))

    def test_acceptance_requires_exact_declared_pairs(self):
        manifest = {
            "release_policy": {"candidate": {"acceptance": [{"repository": "repo", "kind": "ui"}]}}
        }
        self.assertEqual(
            SUITE.acceptance_errors(
                manifest,
                "candidate",
                {"status": "passed", "records": []},
            ),
            ["missing acceptance record repo/ui"],
        )

    def test_command_filter_selects_only_release_channel_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = [sys.executable, "-c", "print('ok')"]
            manifest = {
                "repositories": [
                    {
                        "id": "repo",
                        "path": "repo",
                        "verification": [
                            {"id": "candidate", "cwd": ".", "command": command, "tier": "quick", "platforms": ["macos", "linux"], "required_for": ["candidate"]},
                            {"id": "stable", "cwd": ".", "command": command, "tier": "full", "platforms": ["macos", "linux"], "required_for": ["stable"]},
                        ],
                    }
                ]
            }
            candidate = SUITE.command_records(manifest, root, tier=None, required_for="candidate")
            stable = SUITE.command_records(manifest, root, tier=None, required_for="stable")
        self.assertEqual([record["id"] for record in candidate], ["candidate"])
        self.assertEqual([record["id"] for record in stable], ["stable"])

    def test_release_stable_uses_candidate_snapshot_for_workspace_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "workspace"
            workspace.mkdir()
            (workspace / ".orpheus-suite-workspace").write_text(
                "ORP-SUITE-20260805-001\n", encoding="utf-8"
            )
            repo, revision, remote = self.make_repo(workspace)
            (repo / "artifact.txt").write_text("development\n", encoding="utf-8")
            self.git(repo, "add", "artifact.txt")
            self.git(repo, "commit", "-m", "test: advance development fixture")
            development_revision = self.git(repo, "rev-parse", "HEAD")
            self.git(repo, "push", "origin", "main")
            self.git(repo, "checkout", revision)

            manifest = self.manifest(workspace, revision, remote)
            manifest["release_channel"] = "development"
            manifest["channels"]["development"]["snapshot_id"] = "development"
            manifest["snapshots"]["development"] = {
                **copy.deepcopy(manifest["snapshots"]["candidate"]),
                "id": "development",
                "channel": "development",
                "state": "observed",
                "repositories": {
                    "repo": {
                        "commit": development_revision,
                        "branch": "main",
                        "tag": None,
                    }
                },
            }
            manifest_dir = root / "suite"
            manifest_dir.mkdir()
            manifest_path = manifest_dir / "orpheus-suite.json"
            manifest_path.write_text(
                json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
            )
            acceptance_path = root / "stable.json"
            acceptance_path.write_text(
                json.dumps(
                    {
                        "status": "passed",
                        "records": [
                            {
                                "repository": "repo",
                                "kind": "stable-hardware",
                                "status": "passed",
                                "evidence": "stable.txt",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            before = copy.deepcopy(manifest["snapshots"]["candidate"])
            args = SimpleNamespace(
                command="release",
                release_action="stable",
                manifest=str(manifest_path),
                workspace_root=str(workspace),
                from_snapshot=None,
                channel=None,
                version=None,
                snapshot_id=None,
                acceptance=str(acceptance_path),
                run_checks=True,
                repositories=None,
                apply=True,
                yes=True,
                json=True,
            )
            self.assertEqual(SUITE.cmd_release(args), 0)
            after = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(after["snapshots"]["candidate"], before)
        self.assertEqual(after["channels"]["stable"]["snapshot_id"], "candidate")
        self.assertEqual(after["release_channel"], "stable")

    def test_snapshot_observe_reports_revisions_and_preserves_other_snapshots(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "workspace"
            workspace.mkdir()
            (workspace / ".orpheus-suite-workspace").write_text(
                "ORP-SUITE-20260805-001\n", encoding="utf-8"
            )
            repo, revision, remote = self.make_repo(workspace)
            manifest = self.manifest(workspace, revision, remote)
            manifest_dir = root / "suite"
            manifest_dir.mkdir()
            manifest_path = manifest_dir / "orpheus-suite.json"
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            args = SimpleNamespace(
                command="snapshot",
                snapshot_action="observe",
                manifest=str(manifest_path),
                workspace_root=str(workspace),
                snapshot_id="observed",
                version="0.1.0",
                evidence="suite/evidence/observed.txt",
                apply=False,
                yes=False,
                json=True,
            )
            self.assertEqual(SUITE.cmd_snapshot_observe(args), 0)
            args.apply = True
            args.yes = True
            self.assertEqual(SUITE.cmd_snapshot_observe(args), 0)
            after = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(after["channels"]["development"]["snapshot_id"], "observed")
        self.assertEqual(after["snapshots"]["observed"]["state"], "observed")
        self.assertTrue(after["snapshots"]["observed"]["immutable"])
        self.assertEqual(after["snapshots"]["observed"]["repositories"]["repo"]["commit"], revision)
        self.assertEqual(after["snapshots"]["candidate"], manifest["snapshots"]["candidate"])

    def test_status_errors_reject_unreachable_selected_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, _, remote = self.make_repo(root)
            (repo / "artifact.txt").write_text("unpublished\n", encoding="utf-8")
            self.git(repo, "add", "artifact.txt")
            self.git(repo, "commit", "-m", "test: create unpublished revision")
            unpublished = self.git(repo, "rev-parse", "HEAD")
            manifest = self.manifest(root, unpublished, remote)
            status = SUITE.snapshot_status(manifest, root, None)
        self.assertEqual(status["repositories"][0]["reachability"]["status"], "unreachable")
        self.assertIn("repo: selected revision is unreachable", SUITE.status_errors(status))

    def test_apply_requires_marked_isolated_workspace(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(SUITE.SuiteError):
                SUITE.require_isolated_workspace(Path(directory))


if __name__ == "__main__":
    unittest.main()
