#!/usr/bin/env python3
"""Orpheus Suite synchronization, provenance, and release coordinator.

The manifest is deliberately boring JSON and this tool uses only Python's
standard library. Read-only commands are the default. Commands that can move a
checkout, stage a pin, or publish a release require both --apply and --yes.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict, deque
from pathlib import Path
from typing import Any, Iterable

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "suite" / "orpheus-suite.json"
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
SCHEMA_VERSION = 1


class SuiteError(RuntimeError):
    """A user-actionable suite coordination failure."""


class CommandResult:
    def __init__(self, command: list[str], cwd: Path, completed: subprocess.CompletedProcess[str]):
        self.command = command
        self.cwd = cwd
        self.completed = completed

    @property
    def ok(self) -> bool:
        return self.completed.returncode == 0

    @property
    def output(self) -> str:
        return (self.completed.stdout or "") + (self.completed.stderr or "")


def run_command(
    command: list[str],
    cwd: Path,
    *,
    check: bool = False,
    environment: dict[str, str] | None = None,
) -> CommandResult:
    env = os.environ.copy()
    if environment:
        env.update(environment)
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        if check:
            raise SuiteError(f"cannot run {' '.join(command)} in {cwd}: {exc}") from exc
        completed = subprocess.CompletedProcess(command, 127, "", str(exc))
    result = CommandResult(command, cwd, completed)
    if check and not result.ok:
        raise SuiteError(
            f"command failed ({result.completed.returncode}): {' '.join(command)}\n"
            f"cwd: {cwd}\n{result.output.strip()}"
        )
    return result


def run_git(repo: Path, args: list[str], *, check: bool = False) -> CommandResult:
    return run_command(["git", "-C", str(repo), *args], repo, check=check)


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SuiteError(f"manifest not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise SuiteError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SuiteError(f"manifest root must be an object: {path}")
    return value


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, indent=2, sort_keys=False) + "\n"
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def normalized_url(value: str) -> str:
    raw = value.strip().rstrip("/").removesuffix(".git")
    if raw.startswith("git@github.com:"):
        path = raw.split(":", 1)[1]
        return f"github.com/{path}".lower()
    parsed = urllib.parse.urlparse(raw)
    if parsed.hostname and parsed.hostname.lower() == "github.com":
        return f"github.com/{parsed.path.strip('/')}".lower()
    return raw


def require_isolated_workspace(workspace: Path) -> None:
    workspace = workspace.resolve()
    if ".orpheus-suite-worktrees" in ROOT.parts:
        index = ROOT.parts.index(".orpheus-suite-worktrees")
        active_root = Path(*ROOT.parts[:index])
    else:
        active_root = ROOT.parent.resolve()
    if workspace == active_root or workspace == ROOT.parent.resolve():
        raise SuiteError(f"refusing to mutate the active suite root: {workspace}")
    marker = workspace / ".orpheus-suite-workspace"
    if not marker.is_file():
        raise SuiteError(
            f"refusing to mutate an unmarked workspace: {workspace}; "
            f"create {marker} in the dedicated suite worktree"
        )


def run_git_mutation(repo: Path, args: list[str], *, check: bool = False) -> CommandResult:
    status = run_git(repo, ["status", "--short", "--branch"])
    if not status.ok:
        raise SuiteError(f"cannot inspect repository before mutation: {repo}")
    return run_git(repo, args, check=check)

def is_commit(value: Any) -> bool:
    return isinstance(value, str) and COMMIT_RE.fullmatch(value) is not None


def validate_manifest(manifest: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if manifest.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"schema_version must be {SCHEMA_VERSION}")
    for key in (
        "suite_id",
        "suite_version",
        "release_channel",
        "workspace",
        "channels",
        "repositories",
        "dependencies",
        "release_policy",
        "snapshots",
    ):
        if key not in manifest:
            errors.append(f"missing required top-level key: {key}")
    if manifest.get("release_channel") not in {"development", "candidate", "stable"}:
        errors.append("release_channel must be development, candidate, or stable")
    workspace = manifest.get("workspace")
    if not isinstance(workspace, dict) or workspace.get("paths_are_relative") is not True:
        errors.append("workspace.paths_are_relative must be true")

    repositories = manifest.get("repositories")
    repo_ids: set[str] = set()
    if not isinstance(repositories, list) or not repositories:
        errors.append("repositories must be a non-empty array")
        repositories = []
    for index, repo in enumerate(repositories):
        prefix = f"repositories[{index}]"
        if not isinstance(repo, dict):
            errors.append(f"{prefix} must be an object")
            continue
        repo_id = repo.get("id")
        if not isinstance(repo_id, str) or not repo_id:
            errors.append(f"{prefix}.id must be a non-empty string")
        elif repo_id in repo_ids:
            errors.append(f"duplicate repository id: {repo_id}")
        else:
            repo_ids.add(repo_id)
        path = repo.get("path")
        if not isinstance(path, str) or not path or Path(path).is_absolute() or ".." in Path(path).parts:
            errors.append(f"{prefix}.path must be a safe relative path")
        remote = repo.get("remote")
        if not isinstance(remote, dict) or not remote.get("fetch_url"):
            errors.append(f"{prefix}.remote must declare fetch_url")
        elif not isinstance(remote.get("fetch_remote"), str) or not remote["fetch_remote"]:
            errors.append(f"{prefix}.remote must declare fetch_remote")
        elif (remote.get("push_url") is None) != (remote.get("push_remote") is None):
            errors.append(f"{prefix}.remote must declare push_url and push_remote together, or neither")
        if not isinstance(repo.get("verification"), list):
            errors.append(f"{prefix}.verification must be an array")
        for check_index, check in enumerate(repo.get("verification", [])):
            check_prefix = f"{prefix}.verification[{check_index}]"
            if not isinstance(check, dict):
                errors.append(f"{check_prefix} must be an object")
                continue
            environment = check.get("environment", {})
            if not isinstance(environment, dict) or any(
                not isinstance(key, str) or not isinstance(value, str)
                for key, value in environment.items()
            ):
                errors.append(f"{check_prefix}.environment must map strings to strings")
            required_for = check.get("required_for")
            if not isinstance(required_for, list) or not required_for:
                errors.append(f"{check_prefix}.required_for must be a non-empty array")
            elif any(value not in {"candidate", "stable"} for value in required_for):
                errors.append(f"{check_prefix}.required_for contains an unknown release channel")
        for artifact_index, artifact in enumerate(repo.get("generated_artifacts", [])):
            artifact_prefix = f"{prefix}.generated_artifacts[{artifact_index}]"
            if not isinstance(artifact, dict):
                errors.append(f"{artifact_prefix} must be an object")
                continue
            if not isinstance(artifact.get("expected_sha256"), str) or not re.fullmatch(
                r"[a-f0-9]{64}", artifact.get("expected_sha256", "")
            ):
                errors.append(f"{artifact_prefix}.expected_sha256 must be a lowercase SHA-256")
            source_repository = artifact.get("source_repository")
            if source_repository is not None and source_repository not in repo_ids and not any(
                isinstance(candidate, dict) and candidate.get("id") == source_repository
                for candidate in repositories
            ):
                errors.append(f"{artifact_prefix}.source_repository is unknown: {source_repository}")
            for source_key in ("source_revision", "current_source_revision"):
                if source_key in artifact and not is_commit(artifact[source_key]):
                    errors.append(f"{artifact_prefix}.{source_key} must be a 40-character commit")
        for pin_index, pin in enumerate(repo.get("dependency_pins", [])):
            if not isinstance(pin, dict) or not is_commit(pin.get("revision")):
                errors.append(f"{prefix}.dependency_pins[{pin_index}].revision must be a 40-character commit")

    dependencies = manifest.get("dependencies")
    if not isinstance(dependencies, list):
        errors.append("dependencies must be an array")
        dependencies = []
    graph: dict[str, set[str]] = defaultdict(set)
    for index, dependency in enumerate(dependencies):
        prefix = f"dependencies[{index}]"
        if not isinstance(dependency, dict):
            errors.append(f"{prefix} must be an object")
            continue
        source = dependency.get("from")
        target = dependency.get("to")
        if source not in repo_ids:
            errors.append(f"{prefix}.from is unknown: {source}")
        if target not in repo_ids:
            errors.append(f"{prefix}.to is unknown: {target}")
        if source == target and source in repo_ids:
            errors.append(f"{prefix} cannot point to itself: {source}")
        if source in repo_ids and target in repo_ids:
            graph[source].add(target)
    try:
        topological_order(repo_ids, graph)
    except SuiteError as exc:
        errors.append(str(exc))

    release_policy = manifest.get("release_policy")
    if not isinstance(release_policy, dict):
        errors.append("release_policy must be an object")
        release_policy = {}
    else:
        for gate_name in ("candidate", "stable"):
            gate = release_policy.get(gate_name)
            if not isinstance(gate, dict):
                errors.append(f"release_policy.{gate_name} must be an object")
                continue
            acceptance = gate.get("acceptance")
            if not isinstance(acceptance, list) or not acceptance:
                errors.append(f"release_policy.{gate_name}.acceptance must be a non-empty array")
                continue
            seen: set[tuple[str, str]] = set()
            for index, record in enumerate(acceptance):
                prefix = f"release_policy.{gate_name}.acceptance[{index}]"
                if not isinstance(record, dict):
                    errors.append(f"{prefix} must be an object")
                    continue
                if set(record) != {"repository", "kind"}:
                    errors.append(f"{prefix} must contain exactly repository and kind")
                repository = record.get("repository")
                kind = record.get("kind")
                if not isinstance(repository, str) or not repository:
                    errors.append(f"{prefix}.repository must be a non-empty string")
                elif repository not in repo_ids:
                    errors.append(f"{prefix}.repository is unknown: {repository}")
                if not isinstance(kind, str) or not kind:
                    errors.append(f"{prefix}.kind must be a non-empty string")
                if isinstance(repository, str) and isinstance(kind, str):
                    pair = (repository, kind)
                    if pair in seen:
                        errors.append(f"{prefix} duplicates acceptance pair {repository}/{kind}")
                    seen.add(pair)

    channels = manifest.get("channels", {})
    snapshots = manifest.get("snapshots", {})
    if not isinstance(snapshots, dict):
        errors.append("snapshots must be an object keyed by snapshot id")
        snapshots = {}
    if isinstance(channels, dict):
        for channel in ("development", "candidate", "stable"):
            value = channels.get(channel)
            if not isinstance(value, dict):
                errors.append(f"channels.{channel} must be an object")
                continue
            snapshot_id = value.get("snapshot_id")
            if snapshot_id is not None and snapshot_id not in snapshots:
                errors.append(f"channels.{channel}.snapshot_id is unknown: {snapshot_id}")
            if value.get("mode") == "pinned" and snapshot_id is None and channel == "development":
                errors.append("channels.development cannot be pinned without a snapshot")
    for snapshot_id, snapshot in snapshots.items():
        prefix = f"snapshots.{snapshot_id}"
        if not isinstance(snapshot, dict):
            errors.append(f"{prefix} must be an object")
            continue
        if snapshot.get("id") != snapshot_id:
            errors.append(f"{prefix}.id must match its key")
        if snapshot.get("immutable") is not True:
            errors.append(f"{prefix}.immutable must be true")
        snapshot_repositories = snapshot.get("repositories")
        if not isinstance(snapshot_repositories, dict) or set(snapshot_repositories) != repo_ids:
            errors.append(f"{prefix}.repositories must contain exactly every repository")
        else:
            for repo_id, pin in snapshot_repositories.items():
                if not isinstance(pin, dict) or not is_commit(pin.get("commit")):
                    errors.append(f"{prefix}.repositories.{repo_id}.commit must be a 40-character commit")
                for dependency, revision in (
                    pin.get("dependency_pins", {}) if isinstance(pin, dict) else {}
                ).items():
                    if not is_commit(revision):
                        errors.append(f"{prefix}.repositories.{repo_id}.dependency_pins.{dependency} must be a commit")
        acceptance = snapshot.get("human_acceptance")
        if not isinstance(acceptance, dict) or set(acceptance) != {"status", "records"}:
            errors.append(f"{prefix}.human_acceptance must contain exactly status and records")
            continue
        if acceptance.get("status") not in {"pending", "passed", "failed"}:
            errors.append(f"{prefix}.human_acceptance.status is invalid")
        records = acceptance.get("records")
        if not isinstance(records, list):
            errors.append(f"{prefix}.human_acceptance.records must be an array")
            continue
        seen_acceptance: set[tuple[Any, Any]] = set()
        for index, record in enumerate(records):
            record_prefix = f"{prefix}.human_acceptance.records[{index}]"
            if not isinstance(record, dict) or set(record) != {
                "repository",
                "kind",
                "status",
                "evidence",
            }:
                errors.append(f"{record_prefix} must contain exactly repository, kind, status, evidence")
                continue
            repository = record["repository"]
            kind = record["kind"]
            if not isinstance(repository, str) or not repository:
                errors.append(f"{record_prefix}.repository must be a non-empty string")
            elif repository not in repo_ids:
                errors.append(f"{record_prefix}.repository is unknown: {repository}")
            if not isinstance(kind, str) or not kind:
                errors.append(f"{record_prefix}.kind must be a non-empty string")
            if isinstance(repository, str) and isinstance(kind, str):
                pair = (repository, kind)
                if pair in seen_acceptance:
                    errors.append(f"{record_prefix} duplicates {repository}/{kind}")
                seen_acceptance.add(pair)
            if record["status"] not in {"pending", "passed", "failed"}:
                errors.append(f"{record_prefix}.status is invalid")
            if (
                not isinstance(record["evidence"], str)
                or not record["evidence"].strip()
                or any(character in record["evidence"] for character in "\r\n\0")
            ):
                errors.append(f"{record_prefix}.evidence must be a durable path or URL")
    release_order = release_policy.get("order", [])
    if set(release_order) != repo_ids or len(release_order) != len(repo_ids):
        errors.append("release_policy.order must contain every repository exactly once")
    else:
        positions = {repo_id: index for index, repo_id in enumerate(release_order)}
        for source, targets in graph.items():
            for target in targets:
                if positions[source] >= positions[target]:
                    errors.append(f"release_policy.order must place {source} before {target}")
    return errors


def topological_order(repo_ids: Iterable[str], graph: dict[str, set[str]]) -> list[str]:
    nodes = set(repo_ids)
    indegree = {node: 0 for node in nodes}
    for source in nodes:
        for target in graph.get(source, set()):
            if target in nodes:
                indegree[target] += 1
    queue = deque(sorted(node for node, degree in indegree.items() if degree == 0))
    order: list[str] = []
    while queue:
        source = queue.popleft()
        order.append(source)
        for target in sorted(graph.get(source, set())):
            indegree[target] -= 1
            if indegree[target] == 0:
                queue.append(target)
    if len(order) != len(nodes):
        raise SuiteError("dependency graph contains a cycle")
    return order


def manifest_context(args: argparse.Namespace) -> tuple[Path, dict[str, Any], Path]:
    manifest_path = Path(args.manifest).expanduser().resolve() if args.manifest else DEFAULT_MANIFEST
    manifest = load_json(manifest_path)
    errors = validate_manifest(manifest)
    if errors and args.command not in {"validate"}:
        raise SuiteError("manifest validation failed:\n- " + "\n- ".join(errors))
    if args.workspace_root:
        workspace = Path(args.workspace_root).expanduser().resolve()
    else:
        environment = manifest.get("workspace", {}).get("root_environment")
        configured = os.environ.get(environment) if environment else None
        base = manifest_path.parent.parent
        workspace = Path(configured).expanduser().resolve() if configured else (base / manifest.get("workspace", {}).get("default_root", "..")).resolve()
    return manifest_path, manifest, workspace


def repo_map(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {repo["id"]: repo for repo in manifest.get("repositories", [])}


def selected_snapshot(manifest: dict[str, Any], snapshot_id: str | None, channel: str | None) -> dict[str, Any] | None:
    chosen = snapshot_id
    if chosen is None:
        selected_channel = channel or manifest.get("release_channel", "development")
        chosen = manifest.get("channels", {}).get(selected_channel, {}).get("snapshot_id")
    if chosen is None:
        return None
    snapshot = manifest.get("snapshots", {}).get(chosen)
    if snapshot is None:
        raise SuiteError(f"snapshot not found: {chosen}")
    return snapshot
def remote_branch_revision(path: Path, repo: dict[str, Any]) -> str | None:
    remote = repo.get("remote", {})
    fetch_remote = remote.get("fetch_remote", "origin")
    branch = repo.get("default_branch", "main")
    result = run_git(path, ["rev-parse", "--verify", f"{fetch_remote}/{branch}"])
    return result.completed.stdout.strip() if result.ok else None


def revision_reachable(path: Path, repo: dict[str, Any], revision: str | None) -> dict[str, Any]:
    remote = repo.get("remote", {})
    fetch_remote = remote.get("fetch_remote", "origin")
    branch = repo.get("default_branch", "main")
    remote_revision = remote_branch_revision(path, repo)
    result: dict[str, Any] = {
        "revision": revision,
        "remote": fetch_remote,
        "branch": branch,
        "remote_revision": remote_revision,
        "status": "unavailable",
    }
    if not revision or not is_commit(revision):
        result["status"] = "invalid"
    elif not git_revision_exists(path, revision):
        result["status"] = "missing"
    elif remote_revision is None:
        result["status"] = "remote-branch-unavailable"
    else:
        result["status"] = "reachable" if git_is_ancestor(path, revision, remote_revision) else "unreachable"
    return result


def repo_path(workspace: Path, repo: dict[str, Any]) -> Path:
    return (workspace / repo["path"]).resolve()


def git_head(path: Path) -> str | None:
    result = run_git(path, ["rev-parse", "HEAD"])
    return result.completed.stdout.strip() if result.ok else None


def git_branch(path: Path) -> str:
    result = run_git(path, ["branch", "--show-current"])
    return result.completed.stdout.strip() if result.ok else ""


def git_dirty(path: Path) -> bool:
    return bool(run_git(path, ["status", "--porcelain=v1", "--untracked-files=all"]).completed.stdout.strip())


def git_revision_exists(path: Path, revision: str) -> bool:
    return run_git(path, ["cat-file", "-e", f"{revision}^{{commit}}"]).ok


def git_is_ancestor(path: Path, older: str, newer: str = "HEAD") -> bool:
    return run_git(path, ["merge-base", "--is-ancestor", older, newer]).ok


def remote_url(path: Path, remote: str, *, push: bool = False) -> str | None:
    arguments = ["remote", "get-url"]
    if push:
        arguments.append("--push")
    arguments.append(remote)
    result = run_git(path, arguments)
    return result.completed.stdout.strip() if result.ok else None


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_tree(path: Path, excluded: set[str]) -> str:
    digest = hashlib.sha256()
    for child in sorted(candidate for candidate in path.rglob("*") if candidate.is_file()):
        relative = child.relative_to(path).as_posix()
        if relative in excluded:
            continue
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        with child.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        digest.update(b"\0")
    return digest.hexdigest()


def check_artifact(artifact: dict[str, Any], owner_path: Path, workspace: Path) -> dict[str, Any]:
    path = owner_path / artifact["path"]
    result: dict[str, Any] = {
        "id": artifact["id"],
        "path": str(path),
        "expected_sha256": artifact["expected_sha256"],
        "status": "missing",
    }
    if path.exists():
        if artifact["hash_type"] == "sha256-file-v1":
            actual = hash_file(path) if path.is_file() else None
        else:
            actual = hash_tree(path, set(artifact.get("exclude_paths", []))) if path.is_dir() else None
        result["actual_sha256"] = actual
        result["status"] = "fresh" if actual == artifact["expected_sha256"] else "hash-mismatch"

    source_repository = artifact.get("source_repository")
    source_revision = artifact.get("source_revision")
    if source_repository and source_revision:
        source_path = workspace / source_repository
        result["source_repository"] = source_repository
        result["source_revision"] = source_revision
        if not source_path.is_dir():
            result["provenance_status"] = "source-unavailable"
        else:
            current_source_revision = artifact.get("current_source_revision")
            actual_source_revision = git_head(source_path)
            if current_source_revision and actual_source_revision == current_source_revision:
                result["provenance_status"] = "source-current"
            elif not git_revision_exists(source_path, source_revision):
                result["provenance_status"] = "source-revision-missing"
            elif artifact.get("source_paths") and git_is_ancestor(source_path, source_revision):
                changed = run_git(
                    source_path,
                    ["diff", "--quiet", source_revision, "HEAD", "--", *artifact["source_paths"]],
                )
                result["provenance_status"] = "source-clean" if changed.ok else "source-changed"
            else:
                result["provenance_status"] = "source-revision-not-ancestor"
    return result


def snapshot_status(manifest: dict[str, Any], workspace: Path, snapshot: dict[str, Any] | None) -> dict[str, Any]:
    repositories = repo_map(manifest)
    repo_results: list[dict[str, Any]] = []
    for repo_id, repo in repositories.items():
        path = repo_path(workspace, repo)
        item: dict[str, Any] = {"id": repo_id, "path": str(path), "status": "missing"}
        if not path.is_dir():
            repo_results.append(item)
            continue
        head = git_head(path)
        expected = snapshot.get("repositories", {}).get(repo_id, {}).get("commit") if snapshot else head
        dirty = git_dirty(path)
        item.update({"head": head, "branch": git_branch(path), "dirty": dirty, "expected": expected})
        item["reachability"] = revision_reachable(path, repo, expected)
        if head is None:
            item["status"] = "not-a-git-repository"
        elif snapshot and expected and head != expected:
            item["status"] = "dirty-drift" if dirty else "drifted"
        elif dirty:
            item["status"] = "dirty"
        else:
            item["status"] = "pinned" if snapshot else "observed"

        remote_config = repo.get("remote", {})
        fetch_remote = remote_config.get("fetch_remote", "origin")
        fetch_url = remote_config.get("fetch_url")
        configured = remote_url(path, fetch_remote)
        push_remote = remote_config.get("push_remote")
        push_url = remote_config.get("push_url")
        if push_remote is None and push_url is None:
            configured_push = None
            push_matches = True
            push_status = "not-required"
        else:
            configured_push = remote_url(path, push_remote, push=True) if push_remote else None
            push_matches = bool(configured_push) and normalized_url(configured_push) == normalized_url(push_url or "")
            push_status = "configured" if push_matches else "mismatch"
        item["remote"] = {
            "fetch_remote": fetch_remote,
            "expected": fetch_url,
            "configured": configured,
            "matches": bool(configured) and normalized_url(configured) == normalized_url(fetch_url or ""),
            "push_remote": push_remote,
            "push_expected": push_url,
            "push_configured": configured_push,
            "push_matches": push_matches,
            "push_status": push_status,
        }

        pin_results: list[dict[str, Any]] = []
        expected_pins = snapshot.get("repositories", {}).get(repo_id, {}).get("dependency_pins", {}) if snapshot else {}
        for pin in repo.get("dependency_pins", []):
            dependency = pin.get("dependency")
            expected_pin = expected_pins.get(dependency, pin.get("revision"))
            pin_result: dict[str, Any] = {
                "dependency": dependency,
                "kind": pin.get("kind"),
                "expected": expected_pin,
            }
            if pin.get("kind") == "git-submodule":
                target = path / pin["path"]
            else:
                target = workspace / pin.get("workspace_path", "")
            actual = git_head(target) if target.is_dir() else None
            pin_result.update({"actual": actual, "status": "pinned" if actual == expected_pin else "drifted"})
            dependency_repo = repositories.get(dependency)
            if dependency_repo:
                pin_result["reachability"] = revision_reachable(target, dependency_repo, expected_pin)
            if pin.get("config"):
                cache_values: list[str] = []
                for cache_path in (path / "build-release" / "CMakeCache.txt", path / "build" / "CMakeCache.txt"):
                    if not cache_path.is_file():
                        continue
                    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
                        if line.startswith(f"{pin['config']}:") and "=" in line:
                            cache_values.append(line.split("=", 1)[1])
                expected_path = str((workspace / pin.get("configured_path", pin.get("workspace_path", ""))).resolve())
                if not cache_values:
                    pin_result["resolution_status"] = "unavailable"
                elif expected_path in [str(Path(value).expanduser().resolve()) for value in cache_values]:
                    pin_result["resolution_status"] = "configured"
                else:
                    pin_result["resolution_status"] = "mismatch"
                    pin_result["configured_values"] = cache_values
            pin_results.append(pin_result)
        item["dependency_pins"] = pin_results
        item["artifacts"] = [
            check_artifact(artifact, path, workspace)
            for artifact in repo.get("generated_artifacts", [])
        ]
        repo_results.append(item)
    return {"snapshot": snapshot.get("id") if snapshot else None, "repositories": repo_results}


def status_errors(status: dict[str, Any], *, require_clean: bool = False) -> list[str]:
    errors: list[str] = []
    for repo in status["repositories"]:
        if repo["status"] in {"missing", "not-a-git-repository", "drifted", "dirty-drift"}:
            errors.append(f"{repo['id']}: {repo['status']}")
        if require_clean and repo.get("dirty"):
            errors.append(f"{repo['id']}: worktree is dirty")
        reachability = repo.get("reachability", {})
        if reachability and reachability.get("status") != "reachable":
            errors.append(f"{repo['id']}: selected revision is {reachability.get('status')}")
        if repo.get("remote", {}).get("matches") is False:
            errors.append(f"{repo['id']}: configured fetch remote does not match manifest")
        if repo.get("remote", {}).get("push_status") == "mismatch":
            errors.append(f"{repo['id']}: push remote does not match manifest")
        for pin in repo.get("dependency_pins", []):
            if pin.get("status") != "pinned":
                errors.append(f"{repo['id']}: {pin['dependency']} pin is {pin['status']}")
            if pin.get("resolution_status") in {"mismatch", "unavailable", "not-configured"}:
                errors.append(f"{repo['id']}: {pin['dependency']} resolution is {pin['resolution_status']}")
            pin_reachability = pin.get("reachability", {})
            if pin_reachability and pin_reachability.get("status") != "reachable":
                errors.append(
                    f"{repo['id']}: {pin['dependency']} revision is "
                    f"{pin_reachability.get('status')}"
                )
        for artifact in repo.get("artifacts", []):
            if artifact.get("status") != "fresh":
                errors.append(f"{repo['id']}: artifact {artifact['id']} is {artifact['status']}")
            if artifact.get("provenance_status") not in {None, "source-current", "source-clean"}:
                errors.append(
                    f"{repo['id']}: artifact {artifact['id']} provenance is "
                    f"{artifact.get('provenance_status')}"
                )
    return errors


def output(value: Any, json_mode: bool) -> None:
    if json_mode:
        print(json.dumps(value, indent=2, sort_keys=True))
    elif isinstance(value, str):
        print(value)
    else:
        print(json.dumps(value, indent=2, sort_keys=True))


def command_records(
    manifest: dict[str, Any],
    workspace: Path,
    *,
    tier: str | None,
    selected: set[str] | None = None,
    required_for: str | None = None,
) -> list[dict[str, Any]]:
    current_platform = {
        "Darwin": "macos",
        "Linux": "linux",
        "Windows": "windows",
    }.get(platform.system(), platform.system().lower())
    records: list[dict[str, Any]] = []
    for repo in manifest["repositories"]:
        if selected and repo["id"] not in selected:
            continue
        for check in repo.get("verification", []):
            if tier == "quick" and check["tier"] != "quick":
                continue
            if required_for and required_for not in check["required_for"]:
                continue
            environment = check.get("environment", {})
            if current_platform not in check["platforms"]:
                records.append(
                    {
                        "repository": repo["id"],
                        "id": check["id"],
                        "status": "skipped-platform",
                        "platform": current_platform,
                        "required_for": check["required_for"],
                        "environment": environment,
                    }
                )
                continue
            cwd = workspace / check["cwd"]
            result = run_command(check["command"], cwd, environment=environment)
            status = "passed" if result.ok else "failed"
            if (
                status == "failed"
                and result.completed.returncode == 127
                and ("not found" in result.output.lower() or "no such file" in result.output.lower())
            ):
                status = "unavailable"
            records.append(
                {
                    "repository": repo["id"],
                    "id": check["id"],
                    "status": status,
                    "returncode": result.completed.returncode,
                    "cwd": str(cwd),
                    "command": check["command"],
                    "required_for": check["required_for"],
                    "environment": environment,
                    "output": result.output[-4000:],
                }
            )
    return records


def dependency_graph(manifest: dict[str, Any]) -> dict[str, set[str]]:
    graph: dict[str, set[str]] = defaultdict(set)
    for edge in manifest.get("dependencies", []):
        graph[edge["from"]].add(edge["to"])
    return graph


def affected_ids(manifest: dict[str, Any], changed: Iterable[str]) -> list[str]:
    graph = dependency_graph(manifest)
    affected = set(changed)
    queue = deque(changed)
    while queue:
        source = queue.popleft()
        for target in graph.get(source, set()):
            if target not in affected:
                affected.add(target)
                queue.append(target)
    return [repo_id for repo_id in manifest["release_policy"]["order"] if repo_id in affected]


def clean_repositories(manifest: dict[str, Any], workspace: Path, repo_ids: Iterable[str]) -> list[str]:
    repositories = repo_map(manifest)
    dirty: list[str] = []
    for repo_id in repo_ids:
        path = repo_path(workspace, repositories[repo_id])
        if path.is_dir() and git_dirty(path):
            dirty.append(repo_id)
    return dirty

def require_manifest_owner_clean(manifest_path: Path) -> None:
    owner = manifest_path.parent.parent
    if owner.is_dir() and git_dirty(owner):
        raise SuiteError(f"refusing to overwrite a dirty manifest worktree: {owner}")


def backup_ref(repo_path_value: Path, repo_id: str, stamp: str) -> str:
    ref = f"refs/suite/backups/{stamp}/{repo_id}"
    run_git_mutation(repo_path_value, ["update-ref", ref, "HEAD"], check=True)
    return ref


def update_manifest_pin(manifest: dict[str, Any], repo_id: str, dependency: str, revision: str) -> bool:
    changed = False
    for repo in manifest["repositories"]:
        if repo["id"] != repo_id:
            continue
        for pin in repo.get("dependency_pins", []):
            if pin.get("dependency") == dependency and pin.get("revision") != revision:
                pin["revision"] = revision
                changed = True
    return changed

def snapshot_dependency_operations(
    manifest: dict[str, Any],
    workspace: Path,
    snapshot: dict[str, Any],
    repo_ids: Iterable[str],
) -> list[dict[str, Any]]:
    repositories = repo_map(manifest)
    operations: list[dict[str, Any]] = []
    for repo_id in repo_ids:
        repo = repositories[repo_id]
        expected_pins = snapshot["repositories"][repo_id].get("dependency_pins", {})
        for pin in repo.get("dependency_pins", []):
            revision = expected_pins.get(pin["dependency"], pin["revision"])
            if pin.get("kind") == "git-submodule":
                path = repo_path(workspace, repo) / pin["path"]
            else:
                path = workspace / pin.get("workspace_path", "")
            operations.append(
                {
                    "repository": repo_id,
                    "dependency": pin["dependency"],
                    "kind": pin.get("kind"),
                    "revision": revision,
                    "path": str(path),
                }
            )
    return operations


def capture_snapshot(manifest: dict[str, Any], workspace: Path, snapshot_id: str, suite_version: str, channel: str, state: str, verification: dict[str, Any], acceptance: dict[str, Any]) -> dict[str, Any]:
    repositories: dict[str, Any] = {}
    for repo in manifest["repositories"]:
        path = repo_path(workspace, repo)
        head = git_head(path)
        if not head or not is_commit(head):
            raise SuiteError(f"cannot capture {repo['id']}: no valid HEAD")
        pins: dict[str, str] = {}
        for pin in repo.get("dependency_pins", []):
            if pin.get("kind") == "git-submodule":
                actual = git_head(path / pin["path"])
            else:
                actual = git_head(workspace / pin.get("workspace_path", ""))
            if actual and is_commit(actual):
                pins[pin["dependency"]] = actual
        repositories[repo["id"]] = {"commit": head, "branch": git_branch(path) or "detached", "tag": None, **({"dependency_pins": pins} if pins else {})}
    return {
        "id": snapshot_id,
        "suite_version": suite_version,
        "channel": channel,
        "state": state,
        "immutable": True,
        "observed_at": dt.date.today().isoformat(),
        "repositories": repositories,
        "verification": verification,
        "human_acceptance": acceptance,
    }


def github_repo_from_url(url: str) -> str:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme in {"http", "https"}:
        path = parsed.path.strip("/")
    else:
        path = url.split(":", 1)[-1].strip("/")
    if path.endswith(".git"):
        path = path[:-4]
    if path.count("/") != 1:
        raise SuiteError(f"cannot infer GitHub repository from remote URL: {url}")
    return path


def github_create_pull(repo_name: str, token: str, title: str, head: str, base: str, body: str) -> str:
    payload = json.dumps({"title": title, "head": head, "base": base, "body": body}).encode("utf-8")
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repo_name}/pulls",
        data=payload,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            value = json.loads(response.read().decode("utf-8"))
    except (urllib.error.HTTPError, urllib.error.URLError, json.JSONDecodeError) as exc:
        raise SuiteError(f"GitHub PR creation failed for {repo_name}: {exc}") from exc
    if not isinstance(value, dict) or not value.get("html_url"):
        raise SuiteError(f"GitHub PR response did not contain html_url for {repo_name}")
    return str(value["html_url"])


def cmd_validate(args: argparse.Namespace) -> int:
    manifest_path = Path(args.manifest).expanduser().resolve() if args.manifest else DEFAULT_MANIFEST
    schema_path = manifest_path.parent / "schema" / "orpheus-suite.schema.json"
    errors: list[str] = []
    try:
        manifest = load_json(manifest_path)
        errors.extend(validate_manifest(manifest))
    except SuiteError as exc:
        errors.append(str(exc))
    try:
        schema = load_json(schema_path)
        schema_version = schema.get("properties", {}).get("schema_version", {}).get("const")
        if schema_version != SCHEMA_VERSION:
            errors.append(f"schema version contract must declare {SCHEMA_VERSION}")
    except SuiteError as exc:
        errors.append(str(exc))
    result = {"manifest": str(manifest_path), "schema": str(schema_path), "valid": not errors, "errors": errors}
    output(result, args.json)
    return 0 if not errors else 1


def cmd_status(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, args.channel)
    result = snapshot_status(manifest, workspace, snapshot)
    result.update({"workspace": str(workspace), "channel": args.channel or manifest.get("release_channel"), "errors": status_errors(result)})
    output(result, args.json)
    return 0 if not result["errors"] else 1


def cmd_doctor(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, args.channel)
    result = snapshot_status(manifest, workspace, snapshot)
    errors = status_errors(result, require_clean=args.require_clean)
    checks: list[dict[str, Any]] = []
    if args.checks:
        selected = set(args.repositories) if args.repositories else None
        checks = command_records(manifest, workspace, tier="quick", selected=selected)
        errors.extend(
            f"{item['repository']}: check {item['id']} {item['status']}"
            for item in checks
            if item["status"] in {"failed", "unavailable", "skipped-platform"}
        )
    result.update({"workspace": str(workspace), "checks": checks, "errors": errors, "status": "failed" if errors else "healthy"})
    output(result, args.json)
    return 0 if not errors else 1


def cmd_verify(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    selected = set(args.repositories) if args.repositories else None
    records = command_records(manifest, workspace, tier=None if args.full else "quick", selected=selected)
    failures = [
        record
        for record in records
        if record["status"] in {"failed", "unavailable", "skipped-platform"}
    ]
    result = {"workspace": str(workspace), "tier": "full" if args.full else "quick", "checks": records, "failures": failures, "status": "failed" if failures else "passed"}
    output(result, args.json)
    return 0 if not failures else 1


def cmd_affected(args: argparse.Namespace) -> int:
    _, manifest, _ = manifest_context(args)
    changed = args.repositories
    unknown = sorted(set(changed) - set(repo_map(manifest)))
    if unknown:
        raise SuiteError(f"unknown repositories: {', '.join(unknown)}")
    affected = affected_ids(manifest, changed)
    result = {"changed": changed, "affected": affected, "merge_order": manifest["release_policy"]["order"]}
    output(result, args.json)
    return 0


def cmd_update(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    repositories = repo_map(manifest)
    source = args.repository
    if source not in repositories:
        raise SuiteError(f"unknown repository: {source}")
    if not args.revision or not is_commit(args.revision):
        raise SuiteError("--revision must be a 40-character lowercase commit")
    affected = affected_ids(manifest, [source])
    downstream = [repo_id for repo_id in affected if repo_id != source]
    plan: list[dict[str, Any]] = []
    for repo_id in downstream:
        repo = repositories[repo_id]
        for pin in repo.get("dependency_pins", []):
            if pin.get("dependency") == source:
                plan.append({"repository": repo_id, "dependency": source, "kind": pin.get("kind"), "path": pin.get("path") or pin.get("workspace_path"), "action": "update-exact-pin" if pin.get("kind") == "git-submodule" else "reconfigure-source-or-package"})
    result: dict[str, Any] = {"source": source, "revision": args.revision, "affected": affected, "plan": plan, "applied": [], "backups": []}
    if not args.apply:
        output(result, args.json)
        return 0
    if not args.yes:
        raise SuiteError("update --apply requires --yes; the default is a dry-run")
    require_isolated_workspace(workspace)
    require_manifest_owner_clean(manifest_path)
    dirty = clean_repositories(manifest, workspace, [item["repository"] for item in plan])
    if dirty:
        raise SuiteError("refusing to update dirty worktrees: " + ", ".join(dirty))
    git_items = [item for item in plan if item["kind"] == "git-submodule"]
    for item in git_items:
        consumer = repo_path(workspace, repositories[item["repository"]])
        child = consumer / item["path"]
        if not child.is_dir():
            raise SuiteError(f"submodule path is missing: {child}")
        if not git_revision_exists(child, args.revision):
            run_git_mutation(child, ["fetch", "--no-tags", "origin"], check=True)
        if not git_revision_exists(child, args.revision):
            raise SuiteError(f"revision {args.revision} is not available in {child}")
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    for item in git_items:
        consumer = repo_path(workspace, repositories[item["repository"]])
        result["backups"].append(backup_ref(consumer, item["repository"], stamp))
        child = consumer / item["path"]
        run_git_mutation(child, ["checkout", "--detach", args.revision], check=True)
        run_git_mutation(consumer, ["add", item["path"]], check=True)
        update_manifest_pin(manifest, item["repository"], source, args.revision)
        result["applied"].append(item["repository"])
    if result["applied"]:
        backup_dir = manifest_path.parent / ".backups"
        backup_dir.mkdir(exist_ok=True)
        backup = backup_dir / f"manifest-{dt.datetime.now().strftime('%Y%m%dT%H%M%SZ')}.json"
        shutil.copy2(manifest_path, backup)
        write_json_atomic(manifest_path, manifest)
        result["manifest_backup"] = str(backup)
    output(result, args.json)
    return 0


def cmd_sync(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, args.channel)
    if snapshot is None:
        raise SuiteError("sync requires a pinned snapshot via --snapshot or a pinned channel")
    repositories = repo_map(manifest)
    requested_ids = args.repositories or []
    if args.affected:
        if not requested_ids:
            raise SuiteError("sync --affected requires one or more repository IDs in --repositories")
        target_ids = affected_ids(manifest, requested_ids)
    else:
        target_ids = requested_ids or list(manifest["release_policy"]["order"])
    unknown = sorted(set(target_ids) - set(repositories))
    if unknown:
        raise SuiteError(f"unknown repositories: {', '.join(unknown)}")
    plan = [{"repository": repo_id, "revision": snapshot["repositories"][repo_id]["commit"], "path": str(repo_path(workspace, repositories[repo_id]))} for repo_id in target_ids]
    pin_plan = snapshot_dependency_operations(manifest, workspace, snapshot, target_ids)
    result: dict[str, Any] = {"snapshot": snapshot["id"], "plan": plan, "pin_plan": pin_plan, "applied": [], "applied_pins": [], "backups": []}
    if not args.apply:
        output(result, args.json)
        return 0
    if not args.yes:
        raise SuiteError("sync --apply requires --yes; the default is a dry-run")
    require_isolated_workspace(workspace)
    dirty = clean_repositories(manifest, workspace, target_ids)
    if dirty:
        raise SuiteError("refusing to sync dirty worktrees: " + ", ".join(dirty))
    for item in plan:
        path = Path(item["path"])
        if not path.is_dir():
            raise SuiteError(f"repository path is missing: {path}")
        if not git_revision_exists(path, item["revision"]):
            raise SuiteError(f"revision {item['revision']} is not available locally in {path}; fetch it first")
    for item in pin_plan:
        path = Path(item["path"])
        if not path.is_dir() or not git_revision_exists(path, item["revision"]):
            raise SuiteError(f"dependency revision {item['revision']} is not available at {path}; fetch it first")
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    for item in plan:
        path = Path(item["path"])
        result["backups"].append(backup_ref(path, item["repository"], stamp))
        run_git_mutation(path, ["switch", "--detach", item["revision"]], check=True)
        result["applied"].append(item["repository"])
    for item in pin_plan:
        if item["kind"] == "git-submodule":
            path = Path(item["path"])
            run_git_mutation(path, ["checkout", "--detach", item["revision"]], check=True)
            result["applied_pins"].append(f"{item['repository']}:{item['dependency']}")
    output(result, args.json)
    return 0


def acceptance_value(args: argparse.Namespace) -> dict[str, Any]:
    if not args.acceptance:
        return {"status": "pending", "records": []}
    value = load_json(Path(args.acceptance).expanduser().resolve())
    if set(value) != {"status", "records"}:
        raise SuiteError("acceptance file must contain exactly {status, records}")
    if value.get("status") not in {"pending", "passed", "failed"} or not isinstance(
        value.get("records"), list
    ):
        raise SuiteError("acceptance file must contain {status: pending|passed|failed, records: []}")
    return value


def acceptance_requirements(manifest: dict[str, Any], action: str) -> set[tuple[str, str]]:
    gate = manifest.get("release_policy", {}).get(action, {})
    return {
        (record["repository"], record["kind"])
        for record in gate.get("acceptance", [])
        if isinstance(record, dict)
        and isinstance(record.get("repository"), str)
        and isinstance(record.get("kind"), str)
    }


def acceptance_errors(
    manifest: dict[str, Any],
    action: str,
    acceptance: dict[str, Any],
) -> list[str]:
    required = acceptance_requirements(manifest, action)
    errors: list[str] = []
    if not isinstance(acceptance, dict):
        return ["acceptance value must be an object"]
    if set(acceptance) != {"status", "records"}:
        errors.append("acceptance value must contain exactly {status, records}")
    if acceptance.get("status") != "passed":
        errors.append("required human acceptance is not passed")
    records = acceptance.get("records")
    if not isinstance(records, list):
        return errors + ["acceptance records must be an array"]
    seen: set[tuple[str, str]] = set()
    repositories = set(repo_map(manifest))
    for index, record in enumerate(records):
        prefix = f"acceptance record {index}"
        if not isinstance(record, dict):
            errors.append(f"{prefix} is not an object")
            continue
        if set(record) != {"repository", "kind", "status", "evidence"}:
            errors.append(f"{prefix} must contain exactly repository, kind, status, evidence")
        repository = record.get("repository")
        kind = record.get("kind")
        if not isinstance(repository, str) or not repository:
            errors.append(f"{prefix} has no repository")
        elif repository not in repositories:
            errors.append(f"{prefix} names unknown repository {repository}")
        if not isinstance(kind, str) or not kind:
            errors.append(f"{prefix} has no kind")
        if isinstance(repository, str) and isinstance(kind, str):
            pair = (repository, kind)
            if pair in seen:
                errors.append(f"{prefix} duplicates {repository}/{kind}")
            seen.add(pair)
        if record.get("status") != "passed":
            errors.append(f"{prefix} is not passed")
        evidence = record.get("evidence")
        if (
            not isinstance(evidence, str)
            or not evidence.strip()
            or any(character in evidence for character in "\r\n\0")
        ):
            errors.append(f"{prefix} has no durable evidence path or URL")
    missing = sorted(required - seen, key=lambda pair: (str(pair[0]), str(pair[1])))
    extra = sorted(seen - required, key=lambda pair: (str(pair[0]), str(pair[1])))
    errors.extend(f"missing acceptance record {repository}/{kind}" for repository, kind in missing)
    errors.extend(f"unexpected acceptance record {repository}/{kind}" for repository, kind in extra)
    return errors


def cmd_snapshot_observe(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    require_isolated_workspace(workspace)
    if not args.snapshot_id:
        raise SuiteError("snapshot observe requires --snapshot-id")
    if args.snapshot_id in manifest.get("snapshots", {}):
        raise SuiteError(f"snapshot already exists: {args.snapshot_id}")
    if not args.version:
        raise SuiteError("snapshot observe requires --version X.Y.Z")
    if not args.evidence or not args.evidence.strip():
        raise SuiteError("snapshot observe requires --evidence")
    status = snapshot_status(manifest, workspace, None)
    blockers = status_errors(status, require_clean=True)
    verification = {
        "status": "inventory-only" if not blockers else "blocked",
        "evidence": [args.evidence] + [f"workspace: {error}" for error in blockers],
    }
    acceptance = {"status": "pending", "records": []}
    result = {
        "action": "snapshot-observe",
        "snapshot_id": args.snapshot_id,
        "verification": verification,
        "human_acceptance": acceptance,
        "blockers": blockers,
        "status": status,
        "workspace": str(workspace),
        "applied": False,
    }
    if not args.apply:
        output(result, args.json)
        return 0 if not blockers else 1
    if blockers:
        raise SuiteError("snapshot observe is blocked:\n- " + "\n- ".join(blockers))
    if not args.yes:
        raise SuiteError("snapshot observe --apply requires --yes")
    require_manifest_owner_clean(manifest_path)
    snapshot = capture_snapshot(
        manifest,
        workspace,
        args.snapshot_id,
        args.version,
        "development",
        "observed",
        verification,
        acceptance,
    )
    manifest["snapshots"][args.snapshot_id] = snapshot
    manifest["channels"]["development"]["snapshot_id"] = args.snapshot_id
    backup_dir = manifest_path.parent / ".backups"
    backup_dir.mkdir(exist_ok=True)
    backup = backup_dir / f"manifest-{dt.datetime.now().strftime('%Y%m%dT%H%M%SZ')}.json"
    shutil.copy2(manifest_path, backup)
    write_json_atomic(manifest_path, manifest)
    result.update({"applied": True, "manifest_backup": str(backup)})
    output(result, args.json)
    return 0


def cmd_release(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    require_isolated_workspace(workspace)
    action = args.release_action
    source_snapshot = selected_snapshot(manifest, args.from_snapshot, args.channel)
    if source_snapshot is None:
        raise SuiteError("release requires a source snapshot via --from-snapshot or a pinned channel")
    acceptance = acceptance_value(args)
    acceptance_blockers = acceptance_errors(manifest, action, acceptance)
    selected = set(args.repositories) if args.repositories else None
    checks = (
        command_records(
            manifest,
            workspace,
            tier=None,
            selected=selected,
            required_for=action,
        )
        if args.run_checks
        else []
    )
    failures = [
        item
        for item in checks
        if item["status"] in {"failed", "unavailable", "skipped-platform"}
    ]
    source_status = snapshot_status(manifest, workspace, source_snapshot)
    workspace_blockers = status_errors(source_status, require_clean=True)

    if action == "candidate":
        if not args.version:
            raise SuiteError("release candidate requires --version X.Y.Z")
        snapshot_id = args.snapshot_id or (
            f"candidate-{args.version.replace('.', '-')}-{dt.date.today().isoformat().replace('-', '')}"
        )
        verification = {
            "status": "passed"
            if args.run_checks and not failures and not workspace_blockers and not acceptance_blockers
            else "blocked",
            "evidence": [f"source snapshot: {source_snapshot['id']}"]
            + [f"{item['repository']}/{item['id']}: {item['status']}" for item in checks]
            + [f"workspace: {error}" for error in workspace_blockers],
        }
        blockers: list[str] = []
        if not args.run_checks:
            blockers.append("run declared candidate checks with --run-checks")
        blockers.extend(f"{item['repository']}/{item['id']}: {item['status']}" for item in failures)
        blockers.extend(acceptance_blockers)
        blockers.extend(f"workspace: {error}" for error in workspace_blockers)
        result = {
            "action": action,
            "snapshot_id": snapshot_id,
            "source_snapshot": source_snapshot["id"],
            "verification": verification,
            "human_acceptance": acceptance,
            "blockers": blockers,
            "applied": False,
        }
        if not args.apply:
            output(result, args.json)
            return 0 if not blockers else 1
        if blockers:
            raise SuiteError("release candidate is blocked:\n- " + "\n- ".join(blockers))
        if not args.yes:
            raise SuiteError("release candidate --apply requires --yes")
        require_manifest_owner_clean(manifest_path)
        if snapshot_id in manifest["snapshots"]:
            raise SuiteError(f"snapshot already exists: {snapshot_id}")
        snapshot = capture_snapshot(
            manifest,
            workspace,
            snapshot_id,
            args.version,
            "candidate",
            "candidate",
            verification,
            acceptance,
        )
        manifest["snapshots"][snapshot_id] = snapshot
        manifest["channels"]["candidate"]["snapshot_id"] = snapshot_id
        manifest["suite_version"] = args.version
        backup_dir = manifest_path.parent / ".backups"
        backup_dir.mkdir(exist_ok=True)
        backup = backup_dir / f"manifest-{dt.datetime.now().strftime('%Y%m%dT%H%M%SZ')}.json"
        shutil.copy2(manifest_path, backup)
        write_json_atomic(manifest_path, manifest)
        result.update({"applied": True, "manifest_backup": str(backup)})
        output(result, args.json)
        return 0

    candidate_id = args.from_snapshot or manifest.get("channels", {}).get("candidate", {}).get("snapshot_id")
    candidate = manifest.get("snapshots", {}).get(candidate_id)
    if not candidate or candidate.get("state") != "candidate":
        raise SuiteError("stable promotion requires a candidate snapshot")
    candidate_acceptance_blockers = acceptance_errors(
        manifest,
        "candidate",
        candidate.get("human_acceptance", {}),
    )
    blockers = list(candidate_acceptance_blockers)
    blockers.extend(f"candidate workspace: {error}" for error in workspace_blockers)
    if candidate.get("verification", {}).get("status") != "passed":
        blockers.append("candidate verification is not passed")
    if not args.run_checks:
        blockers.append("run declared stable checks with --run-checks")
    blockers.extend(f"{item['repository']}/{item['id']}: {item['status']}" for item in failures)
    blockers.extend(acceptance_blockers)
    result = {
        "action": action,
        "snapshot_id": candidate["id"],
        "checks": checks,
        "human_acceptance": acceptance,
        "blockers": blockers,
        "applied": False,
    }
    if not args.apply:
        output(result, args.json)
        return 0 if not blockers else 1
    if blockers:
        raise SuiteError("stable promotion is blocked:\n- " + "\n- ".join(blockers))
    if not args.yes:
        raise SuiteError("release stable --apply requires --yes")
    require_manifest_owner_clean(manifest_path)
    manifest["channels"]["stable"]["snapshot_id"] = candidate["id"]
    manifest["release_channel"] = "stable"
    backup_dir = manifest_path.parent / ".backups"
    backup_dir.mkdir(exist_ok=True)
    backup = backup_dir / f"manifest-{dt.datetime.now().strftime('%Y%m%dT%H%M%SZ')}.json"
    shutil.copy2(manifest_path, backup)
    write_json_atomic(manifest_path, manifest)
    result.update({"applied": True, "manifest_backup": str(backup)})
    output(result, args.json)
    return 0


def cmd_rollback(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, None)
    if snapshot is None:
        raise SuiteError(f"snapshot not found: {args.snapshot}")
    repositories = repo_map(manifest)
    target_ids = list(manifest["release_policy"]["order"])
    plan = [{"repository": repo_id, "revision": snapshot["repositories"][repo_id]["commit"], "path": str(repo_path(workspace, repositories[repo_id]))} for repo_id in target_ids]
    pin_plan = snapshot_dependency_operations(manifest, workspace, snapshot, target_ids)
    result: dict[str, Any] = {"snapshot": snapshot["id"], "plan": plan, "pin_plan": pin_plan, "applied": [], "applied_pins": [], "backups": []}
    if not args.apply:
        output(result, args.json)
        return 0
    if not args.yes:
        raise SuiteError("rollback --apply requires --yes; the default is a dry-run")
    require_isolated_workspace(workspace)
    dirty = clean_repositories(manifest, workspace, target_ids)
    if dirty:
        raise SuiteError("refusing to rollback dirty worktrees: " + ", ".join(dirty))
    for item in plan:
        if not git_revision_exists(Path(item["path"]), item["revision"]):
            raise SuiteError(f"rollback revision unavailable locally: {item['repository']} {item['revision']}")
    for item in pin_plan:
        path = Path(item["path"])
        if not path.is_dir() or not git_revision_exists(path, item["revision"]):
            raise SuiteError(f"rollback dependency revision unavailable at {path}: {item['revision']}")
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    for item in plan:
        path = Path(item["path"])
        result["backups"].append(backup_ref(path, item["repository"], stamp))
        run_git_mutation(path, ["switch", "--detach", item["revision"]], check=True)
        result["applied"].append(item["repository"])
    for item in pin_plan:
        if item["kind"] == "git-submodule":
            path = Path(item["path"])
            run_git_mutation(path, ["checkout", "--detach", item["revision"]], check=True)
            result["applied_pins"].append(f"{item['repository']}:{item['dependency']}")
    output(result, args.json)
    return 0


def cmd_coordinate(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    repositories = repo_map(manifest)
    source = args.repository
    if source not in repositories:
        raise SuiteError(f"unknown repository: {source}")
    if not is_commit(args.revision):
        raise SuiteError("--revision must be a 40-character lowercase commit")
    affected = affected_ids(manifest, [source])
    downstream = [repo_id for repo_id in affected if repo_id != source]
    plans: list[dict[str, Any]] = []
    for repo_id in downstream:
        for pin in repositories[repo_id].get("dependency_pins", []):
            if pin.get("dependency") == source:
                plans.append({"repository": repo_id, "kind": pin.get("kind"), "path": pin.get("path") or pin.get("workspace_path"), "action": "create-dependent-pr" if pin.get("kind") == "git-submodule" else "manual-dependent-change"})
    result: dict[str, Any] = {"change_id": args.change_id, "source": source, "revision": args.revision, "affected": affected, "plans": plans, "pull_requests": []}
    if not args.apply:
        output(result, args.json)
        return 0
    if not args.yes:
        raise SuiteError("coordinate --apply requires --yes")
    require_isolated_workspace(workspace)
    token = os.environ.get(args.token_env)
    if not token:
        raise SuiteError(f"coordinate --apply requires {args.token_env} for GitHub PR creation")
    manual = [plan for plan in plans if plan["action"] == "manual-dependent-change"]
    if manual:
        raise SuiteError("automatic coordination cannot safely edit source/package consumers:\n- " + "\n- ".join(plan["repository"] for plan in manual))
    dirty = clean_repositories(manifest, workspace, [plan["repository"] for plan in plans])
    if dirty:
        raise SuiteError("refusing to coordinate dirty worktrees: " + ", ".join(dirty))
    worktrees: list[tuple[Path, Path]] = []
    try:
        for plan in plans:
            consumer = repo_path(workspace, repositories[plan["repository"]])
            repo_name = github_repo_from_url(repositories[plan["repository"]]["remote"]["push_url"])
            branch = f"{manifest['coordination']['branch_prefix']}{args.change_id.lower()}-{plan['repository']}"
            temporary = Path(tempfile.mkdtemp(prefix=f"orpheus-suite-{plan['repository']}-"))
            worktree = temporary / plan["repository"]
            run_git_mutation(
                consumer,
                [
                    "worktree",
                    "add",
                    "-b",
                    branch,
                    str(worktree),
                    f"{repositories[plan['repository']]['remote'].get('fetch_remote', 'origin')}/{repositories[plan['repository']]['default_branch']}",
                ],
                check=True,
            )
            worktrees.append((consumer, worktree))
            run_git_mutation(worktree, ["submodule", "update", "--init", "--recursive", plan["path"]], check=True)
            child = worktree / plan["path"]
            if not git_revision_exists(child, args.revision):
                run_git_mutation(child, ["fetch", "--no-tags", "origin"], check=True)
            if not git_revision_exists(child, args.revision):
                raise SuiteError(f"revision {args.revision} is not available for {plan['repository']}")
            run_git_mutation(child, ["checkout", "--detach", args.revision], check=True)
            run_git_mutation(worktree, ["add", plan["path"]], check=True)
            commit_message = f"suite: pin {source} for {args.change_id}"
            run_git_mutation(worktree, ["commit", "-m", commit_message], check=True)
            push_remote = repositories[plan["repository"]]["remote"].get("push_remote", "origin")
            run_git_mutation(worktree, ["push", push_remote, f"HEAD:{branch}"], check=True)
            body = "\n".join([
                f"Suite change: `{args.change_id}`",
                f"Upstream repository: `{source}`",
                f"Upstream revision: `{args.revision}`",
                "",
                "This PR was created by `tools/suite.py coordinate`.",
                "It is intentionally independent and must be merged only with the coordinated suite change.",
                "",
                "Generated PRs do not trigger another coordinator run.",
            ])
            url = github_create_pull(repo_name, token, f"{manifest['coordination']['pr_title_prefix']} pin {source} ({args.change_id})", branch, repositories[plan["repository"]]["default_branch"], body)
            result["pull_requests"].append({"repository": plan["repository"], "branch": branch, "url": url})
    finally:
        for consumer, worktree in reversed(worktrees):
            run_git_mutation(consumer, ["worktree", "remove", "--force", str(worktree)])
            shutil.rmtree(worktree.parent, ignore_errors=True)
    output(result, args.json)
    return 0


def add_common(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--manifest", help="path to the suite manifest")
    parser.add_argument("--workspace-root", help="workspace containing the declared repository paths")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")


def parser_for() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="suite", description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser("validate", help="validate manifest structure and cross-references")
    add_common(validate)

    status = subparsers.add_parser("status", help="inspect revisions, pins, remotes, and artifacts")
    add_common(status)
    status.add_argument("--channel", choices=["development", "candidate", "stable"])
    status.add_argument("--snapshot")

    doctor = subparsers.add_parser("doctor", help="run non-destructive suite health checks")
    add_common(doctor)
    doctor.add_argument("--channel", choices=["development", "candidate", "stable"])
    doctor.add_argument("--snapshot")
    doctor.add_argument("--checks", action="store_true", help="run declared quick checks")
    doctor.add_argument("--require-clean", action="store_true")
    doctor.add_argument("--repositories", nargs="*")

    verify = subparsers.add_parser("verify", help="run declared verification commands")
    add_common(verify)
    verify.add_argument("--full", action="store_true", help="include full-tier tests")
    verify.add_argument("--repositories", nargs="*")

    affected = subparsers.add_parser("affected", help="calculate downstream repositories in dependency order")
    add_common(affected)
    affected.add_argument("repositories", nargs="+")

    update = subparsers.add_parser("update", help="plan or apply exact downstream dependency-pin updates")
    add_common(update)
    update.add_argument("repository")
    update.add_argument("--revision", required=True)
    update.add_argument("--affected", action="store_true", help="include all downstream repositories in the plan")
    update.add_argument("--apply", action="store_true")
    update.add_argument("--yes", action="store_true")

    sync = subparsers.add_parser("sync", help="plan or apply a pinned snapshot without discarding local work")
    add_common(sync)
    sync.add_argument("--snapshot")
    sync.add_argument("--channel", choices=["development", "candidate", "stable"])
    sync.add_argument("--repositories", nargs="*")
    sync.add_argument("--affected", action="store_true", help="sync the downstream closure of --repositories")
    sync.add_argument("--apply", action="store_true")
    sync.add_argument("--yes", action="store_true")

    snapshot = subparsers.add_parser("snapshot", help="observe an immutable development snapshot")
    snapshot_subparsers = snapshot.add_subparsers(dest="snapshot_action", required=True)
    observe = snapshot_subparsers.add_parser("observe", help="capture current clean reachable revisions")
    add_common(observe)
    observe.add_argument("--snapshot-id", required=True)
    observe.add_argument("--version", required=True)
    observe.add_argument("--evidence", required=True)
    observe.add_argument("--apply", action="store_true")
    observe.add_argument("--yes", action="store_true")

    release = subparsers.add_parser("release", help="plan or apply candidate creation and stable promotion")
    add_common(release)
    release.add_argument("release_action", choices=["candidate", "stable"])
    release.add_argument("--from-snapshot")
    release.add_argument("--channel", choices=["development", "candidate", "stable"])
    release.add_argument("--version")
    release.add_argument("--snapshot-id")
    release.add_argument("--acceptance")
    release.add_argument("--run-checks", action="store_true")
    release.add_argument("--repositories", nargs="*")
    release.add_argument("--apply", action="store_true")
    release.add_argument("--yes", action="store_true")

    rollback = subparsers.add_parser("rollback", help="plan or apply a whole-suite snapshot rollback")
    add_common(rollback)
    rollback.add_argument("snapshot")
    rollback.add_argument("--apply", action="store_true")
    rollback.add_argument("--yes", action="store_true")

    coordinate = subparsers.add_parser("coordinate", help="plan or publish dependent PRs for an upstream revision")
    add_common(coordinate)
    coordinate.add_argument("repository")
    coordinate.add_argument("--revision", required=True)
    coordinate.add_argument("--change-id", required=True)
    coordinate.add_argument("--token-env", default="ORPHEUS_SUITE_GITHUB_TOKEN")
    coordinate.add_argument("--apply", action="store_true")
    coordinate.add_argument("--yes", action="store_true")

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = parser_for()
    args = parser.parse_args(argv)
    try:
        if args.command == "validate":
            return cmd_validate(args)
        if args.command == "status":
            return cmd_status(args)
        if args.command == "doctor":
            return cmd_doctor(args)
        if args.command == "verify":
            return cmd_verify(args)
        if args.command == "affected":
            return cmd_affected(args)
        if args.command == "update":
            return cmd_update(args)
        if args.command == "sync":
            return cmd_sync(args)
        if args.command == "snapshot":
            if args.snapshot_action == "observe":
                return cmd_snapshot_observe(args)
        if args.command == "release":
            return cmd_release(args)
        if args.command == "rollback":
            return cmd_rollback(args)
        if args.command == "coordinate":
            return cmd_coordinate(args)
    except SuiteError as exc:
        if getattr(args, "json", False):
            print(json.dumps({"status": "error", "error": str(exc)}, indent=2))
        else:
            print(f"suite: error: {exc}", file=sys.stderr)
        return 2
    parser.error(f"unhandled command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
