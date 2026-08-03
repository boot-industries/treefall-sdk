#!/usr/bin/env python3
"""Fail-closed Orpheus Suite synchronization, provenance, and release coordinator.

All commands are read-only or dry-run unless both ``--apply`` and ``--yes`` are
provided.  Every mutating command performs one complete, non-mutating preflight
first, records a durable journal before its first mutation, and preserves a
machine-readable recovery envelope when a later operation fails.
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
import stat
import urllib.request
import uuid
from collections import defaultdict, deque
from pathlib import Path
from typing import Any, Iterable, Literal

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from orpheus_artifact_provenance import (  # noqa: E402
    HASH_ALGORITHM,
    ProvenanceError,
    SafePathUnsupported,
    UnsafePath,
    check_manifest as check_provenance_manifest,
    ensure_descriptor_safety,
    inventory_tree,
    open_relative,
    recover_journal as recover_provenance_journal,
    safe_relative_path,
)

DEFAULT_MANIFEST = ROOT / "suite" / "orpheus-suite.json"
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
SEMVER_RE = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$")
ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]+$")
IMMUTABLE_REF_RE = re.compile(r"^refs/tags/[A-Za-z0-9][A-Za-z0-9._/-]*$")
CHANGE_ID_RE = re.compile(r"^ORP-SUITE-[0-9]{8}-[0-9]{3}$")
SCHEMA_VERSION = 1
MAX_OUTPUT = 4000
REMOTE_CACHE: dict[tuple[str, str], dict[str, Any]] = {}
INVENTORY_CACHE: dict[tuple[str, tuple[str, ...], str], tuple[list[str], str]] = {}


class SuiteError(RuntimeError):
    """A user-actionable invocation or manifest failure."""


class ManifestError(SuiteError):
    """The manifest or schema is invalid; no operation may proceed."""


class PartialApply(SuiteError):
    """A journaled apply failed after at least one mutation boundary."""

    def __init__(self, envelope: dict[str, Any]):
        self.envelope = envelope
        super().__init__(envelope.get("error", "suite apply interrupted"))


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
        return ((self.completed.stdout or "") + (self.completed.stderr or ""))[-MAX_OUTPUT:]


def run_command(
    command: list[str],
    cwd: Path,
    *,
    check: bool = False,
    env: dict[str, str] | None = None,
) -> CommandResult:
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


def load_json_value(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SuiteError(f"manifest not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise SuiteError(f"invalid JSON in {path}: {exc}") from exc


def load_json(path: Path) -> dict[str, Any]:
    value = load_json_value(path)
    if not isinstance(value, dict):
        raise SuiteError(f"JSON root must be an object: {path}")
    return value


def _fsync_parent(path: Path) -> None:
    try:
        fd = os.open(str(path), os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError:
        # Some filesystems do not allow directory fsync.  The write remains
        # durable at the file boundary; callers still retain the journal.
        pass


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    """Atomically replace one file and durably flush its parent directory."""
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(value, indent=2, sort_keys=False) + "\n").encode("utf-8")
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        _fsync_parent(path.parent)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def normalized_url(value: str | None) -> str:
    """Normalize GitHub HTTPS, SSH, and scp-style URLs to one identity."""
    if not value:
        return ""
    raw = value.strip().rstrip("/")
    if re.match(r"^[^/@:]+@[^/:]+:.+", raw):
        host, path = raw.split(":", 1)
        host = host.split("@", 1)[-1].lower()
    else:
        parsed = urllib.parse.urlsplit(raw)
        host = (parsed.hostname or "").lower()
        if host == "github.com":
            host = "github.com"
        path = parsed.path
        if parsed.scheme == "file":
            return f"file://{Path(urllib.parse.unquote(path)).resolve()}"
    path = path.strip("/")
    if path.endswith(".git"):
        path = path[:-4]
    if host == "github.com":
        return f"github.com/{path.lower()}"
    return f"{host}/{path}" if host else path


def is_commit(value: Any) -> bool:
    return isinstance(value, str) and COMMIT_RE.fullmatch(value) is not None


def is_safe_relative(value: Any) -> bool:
    try:
        safe_relative_path(value)
        return True
    except (TypeError, UnsafePath):
        return False


def _schema_path(manifest_path: Path) -> Path:
    candidate = manifest_path.parent / "schema" / "orpheus-suite.schema.json"
    return candidate if candidate.is_file() else ROOT / "suite" / "schema" / "orpheus-suite.schema.json"


def _json_path(path: Iterable[Any]) -> str:
    value = "$"
    for component in path:
        value += f"[{component}]" if isinstance(component, int) else f".{component}"
    return value


def schema_validation_errors(value: Any, schema_path: Path) -> list[dict[str, Any]]:
    errors: list[dict[str, Any]] = []
    try:
        import jsonschema
        from jsonschema import Draft202012Validator, FormatChecker
    except ImportError:
        return [{"kind": "schema", "path": "$", "message": "jsonschema==4.23.0 is unavailable"}]
    try:
        schema = load_json_value(schema_path)
        if not isinstance(schema, dict):
            return [{"kind": "schema", "path": "$", "message": "schema root must be an object"}]
        Draft202012Validator.check_schema(schema)
        validator = Draft202012Validator(schema, format_checker=FormatChecker())
        for error in sorted(validator.iter_errors(value), key=lambda item: (list(item.path), item.message)):
            errors.append({
                "kind": "schema",
                "path": _json_path(error.path),
                "message": error.message,
                "validator": error.validator,
            })
    except (SuiteError, jsonschema.exceptions.SchemaError, OSError) as exc:
        errors.append({"kind": "schema", "path": "$", "message": str(exc)})
    return errors


def _business_error(message: str, path: str | None = None) -> dict[str, Any]:
    return {"kind": "business", "path": path or "$", "message": message}


def validate_manifest(manifest: Any) -> list[str]:
    """Validate cross-record invariants after JSON Schema validation.

    This function never assumes nested types, so malformed root/null/wrong-type
    fixtures return diagnostics instead of TypeError/AttributeError.
    """
    errors: list[str] = []
    if not isinstance(manifest, dict):
        return ["manifest root must be an object"]
    if manifest.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"schema_version must be {SCHEMA_VERSION}")
    repositories = manifest.get("repositories")
    if not isinstance(repositories, list):
        return errors + ["repositories must be an array"]
    repo_ids: list[str] = []
    repo_map_value: dict[str, dict[str, Any]] = {}
    artifact_ids: set[str] = set()
    check_ids: set[str] = set()
    for index, repo in enumerate(repositories):
        prefix = f"repositories[{index}]"
        if not isinstance(repo, dict):
            errors.append(f"{prefix} must be an object")
            continue
        repo_id = repo.get("id")
        if not isinstance(repo_id, str) or not ID_RE.fullmatch(repo_id):
            errors.append(f"{prefix}.id must match {ID_RE.pattern}")
        elif repo_id in repo_map_value:
            errors.append(f"duplicate repository id: {repo_id}")
        else:
            repo_ids.append(repo_id)
            repo_map_value[repo_id] = repo
        if not is_safe_relative(repo.get("path")):
            errors.append(f"{prefix}.path must be a safe relative path")
        remote = repo.get("remote")
        if not isinstance(remote, dict) or not isinstance(remote.get("fetch_url"), str) or not remote.get("fetch_url"):
            errors.append(f"{prefix}.remote.fetch_url must be declared")
        elif (remote.get("push_url") is None) != (remote.get("push_remote") is None):
            errors.append(f"{prefix}.remote must declare push_url and push_remote together, or neither")
        for artifact_index, artifact in enumerate(repo.get("generated_artifacts", []) if isinstance(repo.get("generated_artifacts", []), list) else []):
            artifact_prefix = f"{prefix}.generated_artifacts[{artifact_index}]"
            if not isinstance(artifact, dict):
                errors.append(f"{artifact_prefix} must be an object")
                continue
            artifact_id = artifact.get("id")
            if not isinstance(artifact_id, str) or not ID_RE.fullmatch(artifact_id):
                errors.append(f"{artifact_prefix}.id must be a valid artifact id")
            elif artifact_id in artifact_ids:
                errors.append(f"duplicate artifact id: {artifact_id}")
            else:
                artifact_ids.add(artifact_id)
            if not is_safe_relative(artifact.get("path")):
                errors.append(f"{artifact_prefix}.path must be safe and relative")
            scope = artifact.get("scope")
            if scope not in {"public", "private"}:
                errors.append(f"{artifact_prefix}.scope must be public or private")
            for source_key in ("source_revision", "current_source_revision"):
                if source_key in artifact and not is_commit(artifact[source_key]):
                    errors.append(f"{artifact_prefix}.{source_key} must be a 40-character commit")
            source_repository = artifact.get("source_repository")
            if source_repository is not None and source_repository not in repo_map_value and source_repository not in repo_ids:
                # A source may be declared later in the repository array.
                errors.append(f"{artifact_prefix}.source_repository is unknown: {source_repository}")
            has_freshness = any(key in artifact for key in ("source_repository", "source_revision", "current_source_revision", "source_paths", "freshness_check"))
            if has_freshness and not is_safe_relative(artifact.get("provenance_path")):
                errors.append(f"{artifact_prefix}.provenance_path is required and must be safe")
            for source_path in artifact.get("source_paths", []) if isinstance(artifact.get("source_paths"), list) else []:
                if not is_safe_relative(source_path):
                    errors.append(f"{artifact_prefix}.source_paths contains an unsafe path: {source_path}")
            files = artifact.get("files")
            if files is not None:
                if not isinstance(files, list) or any(not is_safe_relative(item) for item in files):
                    errors.append(f"{artifact_prefix}.files must contain safe relative paths")
                elif len(files) != len(set(files)):
                    errors.append(f"{artifact_prefix}.files contains duplicate paths")
                elif files != sorted(files):
                    errors.append(f"{artifact_prefix}.files must be in canonical order")
            if scope == "private":
                forbidden = {"source_paths", "targets", "target_metadata", "component_metadata", "private_url", "source_inventory"}
                found = sorted(forbidden.intersection(artifact))
                if found:
                    errors.append(f"{artifact_prefix} private record exposes forbidden fields: {', '.join(found)}")
                for field in ("path", "provenance_path", "label"):
                    if field in artifact and isinstance(artifact[field], str) and (Path(artifact[field]).is_absolute() or "://" in artifact[field]):
                        errors.append(f"{artifact_prefix} private record field {field} must be a relative label")
        for check_index, check in enumerate(repo.get("verification", []) if isinstance(repo.get("verification", []), list) else []):
            if not isinstance(check, dict):
                continue
            check_id = check.get("id")
            if isinstance(check_id, str):
                if check_id in check_ids:
                    errors.append(f"duplicate check id: {check_id}")
                check_ids.add(check_id)
        for pin_index, pin in enumerate(repo.get("dependency_pins", []) if isinstance(repo.get("dependency_pins", []), list) else []):
            pin_prefix = f"{prefix}.dependency_pins[{pin_index}]"
            if not isinstance(pin, dict) or not is_commit(pin.get("revision")):
                errors.append(f"{pin_prefix}.revision must be a 40-character commit")
            if isinstance(pin, dict) and pin.get("dependency") not in repo_ids and pin.get("dependency") not in repo_map_value:
                # External pins are validated separately; a dependency pin must
                # always name a repository in the suite graph.
                errors.append(f"{pin_prefix}.dependency is unknown: {pin.get('dependency')}")
            for key in ("path", "workspace_path", "configured_path"):
                if isinstance(pin, dict) and key in pin and not is_safe_relative(pin[key]):
                    errors.append(f"{pin_prefix}.{key} must be safe and relative")
        for pin_index, pin in enumerate(repo.get("external_pins", []) if isinstance(repo.get("external_pins", []), list) else []):
            if isinstance(pin, dict) and not is_commit(pin.get("revision")):
                errors.append(f"{prefix}.external_pins[{pin_index}].revision must be a 40-character commit")
            if isinstance(pin, dict) and not is_safe_relative(pin.get("path")):
                errors.append(f"{prefix}.external_pins[{pin_index}].path must be safe and relative")
    known_repos = set(repo_ids)
    dependencies = manifest.get("dependencies")
    graph: dict[str, set[str]] = defaultdict(set)
    seen_edges: set[tuple[Any, Any, Any]] = set()
    if not isinstance(dependencies, list):
        errors.append("dependencies must be an array")
        dependencies = []
    for index, dependency in enumerate(dependencies):
        prefix = f"dependencies[{index}]"
        if not isinstance(dependency, dict):
            errors.append(f"{prefix} must be an object")
            continue
        source, target, kind = dependency.get("from"), dependency.get("to"), dependency.get("kind")
        edge = (source, target, kind)
        if edge in seen_edges:
            errors.append(f"duplicate dependency edge: {source} -> {target} ({kind})")
        seen_edges.add(edge)
        if source not in known_repos:
            errors.append(f"{prefix}.from is unknown: {source}")
        if target not in known_repos:
            errors.append(f"{prefix}.to is unknown: {target}")
        if source == target and source in known_repos:
            errors.append(f"{prefix} cannot point to itself: {source}")
        if source in known_repos and target in known_repos:
            graph[source].add(target)
    try:
        topological_order(known_repos, graph)
    except SuiteError as exc:
        errors.append(str(exc))
    channels = manifest.get("channels")
    snapshots = manifest.get("snapshots")
    if not isinstance(channels, dict):
        errors.append("channels must be an object")
        channels = {}
    if not isinstance(snapshots, dict):
        errors.append("snapshots must be an object keyed by snapshot id")
        snapshots = {}
    for channel in ("development", "candidate", "stable"):
        value = channels.get(channel)
        if not isinstance(value, dict):
            continue
        snapshot_id = value.get("snapshot_id")
        if snapshot_id is not None and snapshot_id not in snapshots:
            errors.append(f"channels.{channel}.snapshot_id is unknown: {snapshot_id}")
        if value.get("mode") == "pinned" and channel == "development" and snapshot_id is None:
            errors.append("channels.development cannot be pinned without a snapshot")
        if snapshot_id in snapshots and isinstance(snapshots[snapshot_id], dict):
            if snapshots[snapshot_id].get("channel") != channel:
                errors.append(f"channels.{channel}.snapshot_id channel disagrees with snapshot")
    for snapshot_id, snapshot in snapshots.items():
        prefix = f"snapshots.{snapshot_id}"
        if not isinstance(snapshot, dict):
            errors.append(f"{prefix} must be an object")
            continue
        if snapshot.get("id") != snapshot_id:
            errors.append(f"{prefix}.id must match its key")
        snapshot_repositories = snapshot.get("repositories")
        if not isinstance(snapshot_repositories, dict) or set(snapshot_repositories) != known_repos:
            errors.append(f"{prefix}.repositories must contain exactly every repository")
        elif isinstance(snapshot_repositories, dict):
            for repo_id, pin in snapshot_repositories.items():
                if not isinstance(pin, dict) or not is_commit(pin.get("commit")):
                    errors.append(f"{prefix}.repositories.{repo_id}.commit must be a 40-character commit")
                if isinstance(pin, dict):
                    immutable_ref = pin.get("immutable_ref")
                    tag = pin.get("tag")
                    if not isinstance(immutable_ref, str) or IMMUTABLE_REF_RE.fullmatch(immutable_ref) is None:
                        errors.append(f"{prefix}.repositories.{repo_id}.immutable_ref must be refs/tags/<immutable-tag>")
                    if not isinstance(tag, str) or not tag or not isinstance(immutable_ref, str) or tag != immutable_ref.removeprefix("refs/tags/"):
                        errors.append(f"{prefix}.repositories.{repo_id}.tag must match immutable_ref")
                    for dependency, revision in (pin.get("dependency_pins", {}) if isinstance(pin.get("dependency_pins", {}), dict) else {}).items():
                        if not is_commit(revision):
                            errors.append(f"{prefix}.repositories.{repo_id}.dependency_pins.{dependency} must be a commit")
    release_policy = manifest.get("release_policy")
    release_order = release_policy.get("order") if isinstance(release_policy, dict) else None
    merge_order = manifest.get("coordination", {}).get("merge_order") if isinstance(manifest.get("coordination"), dict) else None
    if not isinstance(release_order, list) or set(release_order) != known_repos or len(release_order) != len(known_repos):
        errors.append("release_policy.order must contain every repository exactly once")
    if not isinstance(merge_order, list) or merge_order != release_order:
        errors.append("coordination.merge_order must exactly match release_policy.order")
    if isinstance(release_order, list):
        positions = {repo_id: index for index, repo_id in enumerate(release_order)}
        for source, targets in graph.items():
            for target in targets:
                if source in positions and target in positions and positions[source] >= positions[target]:
                    errors.append(f"release_policy.order must place {source} before {target}")
    return errors


def topological_order(repo_ids: Iterable[str], graph: dict[str, set[str]]) -> list[str]:
    nodes = set(repo_ids)
    indegree = {node: 0 for node in nodes}
    order: list[str] = []
    for source in nodes:
        for target in graph.get(source, set()):
            if target in nodes:
                indegree[target] += 1
    queue = deque(sorted(node for node, degree in indegree.items() if degree == 0))
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
    manifest_path = Path(args.manifest).expanduser().absolute() if getattr(args, "manifest", None) else DEFAULT_MANIFEST
    value = load_json_value(manifest_path)
    schema_errors = schema_validation_errors(value, _schema_path(manifest_path))
    business_errors = validate_manifest(value)
    if schema_errors or business_errors:
        details = [f"{item['path']}: {item['message']}" for item in schema_errors] + business_errors
        if getattr(args, "command", None) != "validate":
            raise ManifestError("manifest validation failed:\n- " + "\n- ".join(details))
    if not isinstance(value, dict):
        raise ManifestError("manifest root must be an object")
    if getattr(args, "workspace_root", None):
        workspace = Path(args.workspace_root).expanduser().absolute()
    else:
        environment = value.get("workspace", {}).get("root_environment") if isinstance(value.get("workspace"), dict) else None
        configured = os.environ.get(environment) if environment else None
        base = manifest_path.parent.parent
        workspace_value = value.get("workspace", {}).get("default_root", "..") if isinstance(value.get("workspace"), dict) else ".."
        workspace = Path(configured).expanduser().absolute() if configured else (base / workspace_value).absolute()
    return manifest_path, value, workspace


def repo_map(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {repo["id"]: repo for repo in manifest.get("repositories", []) if isinstance(repo, dict) and isinstance(repo.get("id"), str)}


def selected_snapshot(manifest: dict[str, Any], snapshot_id: str | None, channel: str | None) -> dict[str, Any] | None:
    chosen = snapshot_id
    if chosen is None:
        selected_channel = channel or manifest.get("release_channel", "development")
        channel_value = manifest.get("channels", {}).get(selected_channel, {})
        chosen = channel_value.get("snapshot_id") if isinstance(channel_value, dict) else None
    if chosen is None:
        return None
    snapshot = manifest.get("snapshots", {}).get(chosen)
    if not isinstance(snapshot, dict):
        raise SuiteError(f"snapshot not found: {chosen}")
    return snapshot


def repo_path(workspace: Path, repo: dict[str, Any]) -> Path:
    relative = safe_relative_path(repo["path"])
    return workspace / Path(relative)


def _safe_root(path: Path) -> None:
    ensure_descriptor_safety()
    fd = os.open(str(path), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        if not os.fstat(fd).st_mode & 0o040000:
            raise UnsafePath(f"unsafe-path: not a directory: {path}")
    finally:
        os.close(fd)


def safe_path_state(root: Path, relative: str, *, expect_directory: bool | None = None) -> str:
    try:
        _safe_root(root)
        canonical = safe_relative_path(relative)
        flags = os.O_RDONLY | (os.O_DIRECTORY if expect_directory else 0)
        fd = open_relative(root, canonical, flags=flags)
        try:
            info = os.fstat(fd)
        finally:
            os.close(fd)
        if expect_directory is True and not stat.S_ISDIR(info.st_mode):
            return "unsafe-path"
        if expect_directory is False and not stat.S_ISREG(info.st_mode):
            return "unsafe-path"
        return "present"
    except FileNotFoundError:
        return "missing"
    except SafePathUnsupported:
        return "safe-path-unsupported"
    except UnsafePath as exc:
        return "symlink-escape" if "symlink" in str(exc) else "unsafe-path"
    except OSError:
        return "unavailable"


def git_head(path: Path) -> str | None:
    result = run_git(path, ["rev-parse", "HEAD"])
    return result.completed.stdout.strip() if result.ok else None


def git_branch(path: Path) -> str:
    result = run_git(path, ["branch", "--show-current"])
    return result.completed.stdout.strip() if result.ok else ""


def git_status_paths(path: Path) -> list[str]:
    result = run_git(path, ["status", "--porcelain=v1", "--untracked-files=all"])
    return result.completed.stdout.splitlines() if result.ok else []


def git_dirty(path: Path) -> bool:
    return bool(git_status_paths(path))


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


def hash_tree(path: Path, excluded: set[str]) -> tuple[list[str], str]:
    key = (str(path.absolute()), tuple(sorted(excluded)), "inventory")
    if key not in INVENTORY_CACHE:
        INVENTORY_CACHE[key] = inventory_tree(path, excluded)
    return INVENTORY_CACHE[key]


def _artifact_source_path(artifact: dict[str, Any], workspace: Path, repositories: dict[str, dict[str, Any]]) -> Path | None:
    source_repository = artifact.get("source_repository")
    if not source_repository or source_repository not in repositories:
        return None
    return repo_path(workspace, repositories[source_repository])
def hash_file_relative(root: Path, relative: str) -> str:
    fd = open_relative(root, relative)
    digest = hashlib.sha256()
    try:
        while True:
            chunk = os.read(fd, 1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    finally:
        os.close(fd)
    return digest.hexdigest()


def read_relative_json(root: Path, relative: str) -> Any:
    fd = open_relative(root, relative)
    chunks: list[bytes] = []
    try:
        while True:
            chunk = os.read(fd, 1024 * 1024)
            if not chunk:
                break
            chunks.append(chunk)
    finally:
        os.close(fd)
    return json.loads(b"".join(chunks).decode("utf-8"))


def _provenance_state_from_manifest(manifest_result: dict[str, Any]) -> str:
    return str(manifest_result.get("status", "manifest-invalid"))


def check_artifact(artifact: dict[str, Any], owner_path: Path, workspace: Path, repositories: dict[str, dict[str, Any]] | None = None) -> dict[str, Any]:
    """Check content and source provenance once, preserving cause precedence."""
    repositories = repositories or {}
    result: dict[str, Any] = {
        "id": artifact.get("id"),
        "path": str(owner_path / str(artifact.get("path", ""))),
        "expected_sha256": artifact.get("expected_sha256"),
        "scope": artifact.get("scope"),
        "status": "missing",
        "content": {"status": "missing"},
        "provenance": {},
    }
    artifact_path_value = artifact.get("path")
    if not isinstance(artifact_path_value, str) or not is_safe_relative(artifact_path_value):
        result["status"] = "unsafe-path"
        result["provenance_status"] = "unsafe-path"
        return result
    state = safe_path_state(owner_path, artifact_path_value)
    if state != "present":
        result["status"] = state
        result["provenance_status"] = state if state != "missing" else "inventory-missing"
        result["provenance"] = {"state": result["provenance_status"]}
        return result
    artifact_path = owner_path / artifact_path_value
    try:
        info = artifact_path.lstat()
        if info.st_mode & 0o170000 == 0o120000:
            raise UnsafePath("symlink-escape: artifact is a symlink")
        if stat.S_ISDIR(info.st_mode):
            files, actual = hash_tree(artifact_path, set(artifact.get("exclude_paths", [])))
            content = {"status": "fresh" if actual == artifact.get("expected_sha256") else "hash-mismatch", "actual_sha256": actual, "files": files, "file_count": len(files)}
        elif stat.S_ISREG(info.st_mode):
            actual = hash_file_relative(owner_path, artifact_path_value)
            content = {"status": "fresh" if actual == artifact.get("expected_sha256") else "hash-mismatch", "actual_sha256": actual, "file_count": 1}
        else:
            content = {"status": "unsafe-path"}
    except SafePathUnsupported:
        result["status"] = "safe-path-unsupported"
        result["provenance_status"] = "safe-path-unsupported"
        return result
    except (OSError, UnsafePath) as exc:
        result["status"] = "symlink-escape" if "symlink" in str(exc) else "unsafe-path"
        result["provenance_status"] = result["status"]
        result["provenance"] = {"state": result["status"], "error": str(exc)}
        return result
    result["content"] = content
    result["status"] = content["status"]

    provenance_result: dict[str, Any] | None = None
    provenance_path_value = artifact.get("provenance_path")
    if provenance_path_value:
        if not is_safe_relative(provenance_path_value):
            provenance_result = {"status": "unsafe-path", "details": {"error": "unsafe provenance_path"}}
        elif stat.S_ISDIR(info.st_mode):
            provenance_file = artifact_path / provenance_path_value
            provenance_result = check_provenance_manifest(artifact_path, provenance_file, _artifact_source_path(artifact, workspace, repositories))
    elif any(key in artifact for key in ("source_repository", "source_revision", "source_paths", "freshness_check")):
        provenance_result = {"status": "manifest-invalid", "details": {"error": "provenance_path is required for source freshness"}}

    source_repository = artifact.get("source_repository")
    source_revision = artifact.get("source_revision")
    source_path = _artifact_source_path(artifact, workspace, repositories)
    provenance_details: dict[str, Any] = {}
    generated_source_revision: str | None = None
    generated_manifest: dict[str, Any] | None = None
    if provenance_result:
        provenance_details.update(provenance_result)
        if isinstance(provenance_result.get("details"), dict):
            generated_manifest = provenance_result["details"].get("manifest_value") if isinstance(provenance_result["details"].get("manifest_value"), dict) else None
        if provenance_result.get("status") in {"manifest-invalid", "unsafe-path", "symlink-escape", "safe-path-unsupported"}:
            result["provenance"] = provenance_details
            result["provenance_status"] = provenance_result["status"]
            return result
        try:
            manifest_value = read_relative_json(artifact_path, str(provenance_path_value))
            if isinstance(manifest_value, dict):
                source_record = manifest_value.get("source", manifest_value)
                if isinstance(source_record, dict):
                    generated_source_revision = source_record.get("revision")
                generated_manifest = manifest_value
        except (OSError, json.JSONDecodeError, ProvenanceError):
            pass

    if source_repository and source_revision:
        if source_path is None or not source_path.is_dir():
            provenance_details.update({"state": "source-unavailable", "source_repository": source_repository})
            result["provenance"] = provenance_details
            result["provenance_status"] = "source-unavailable"
            return result
        source_status = git_status_paths(source_path)
        if not git_revision_exists(source_path, source_revision):
            provenance_details.update({"state": "source-revision-missing", "source_revision": source_revision})
            result["provenance"] = provenance_details
            result["provenance_status"] = "source-revision-missing"
            return result
        if not git_is_ancestor(source_path, source_revision):
            provenance_details.update({"state": "source-revision-not-ancestor", "source_revision": source_revision, "head": git_head(source_path)})
            result["provenance"] = provenance_details
            result["provenance_status"] = "source-revision-not-ancestor"
            return result
        if source_status:
            provenance_details.update({"state": "source-dirty", "dirty": True, "paths": source_status})
            result["provenance"] = provenance_details
            result["provenance_status"] = "source-dirty"
            return result
        if generated_source_revision and generated_source_revision != source_revision:
            provenance_details.update({"state": "source-revision-mismatch", "declared_revision": source_revision, "generated_revision": generated_source_revision})
            result["provenance"] = provenance_details
            result["provenance_status"] = "source-revision-mismatch"
            return result
        source_paths = artifact.get("source_paths", [])
        if source_paths:
            unsafe_source_path = next((path for path in source_paths if not is_safe_relative(path)), None)
            if unsafe_source_path:
                provenance_details.update({"state": "unsafe-path", "path": unsafe_source_path})
                result["provenance"] = provenance_details
                result["provenance_status"] = "unsafe-path"
                return result
            changed = run_git(source_path, ["diff", "--quiet", source_revision, "HEAD", "--", *source_paths])
            if not changed.ok:
                provenance_details.update({"state": "source-changed", "source_revision": source_revision, "paths": source_paths})
                result["provenance"] = provenance_details
                result["provenance_status"] = "source-changed"
                return result
        expected_contract = artifact.get("contract_metadata")
        if expected_contract is not None and generated_manifest is not None:
            actual_contract = generated_manifest.get("contract_metadata", generated_manifest.get("design_token_contract_version"))
            if actual_contract != expected_contract:
                provenance_details.update({"state": "contract-metadata-mismatch", "expected": expected_contract, "actual": actual_contract})
                result["provenance"] = provenance_details
                result["provenance_status"] = "contract-metadata-mismatch"
                return result
    if provenance_result and provenance_result.get("status") in {"inventory-missing", "inventory-extra", "inventory-duplicate", "inventory-order"}:
        result["provenance"] = provenance_details
        result["provenance_status"] = provenance_result["status"]
        return result
    if content.get("status") != "fresh":
        provenance_details.update({"state": "content-hash-mismatch", "expected": artifact.get("expected_sha256"), "actual": content.get("actual_sha256")})
        result["provenance"] = provenance_details
        result["provenance_status"] = "content-hash-mismatch"
    else:
        provenance_details.update({"state": "source-current" if source_repository else "content-current"})
        result["provenance"] = provenance_details
        result["provenance_status"] = provenance_details["state"]
    return result


def snapshot_status(manifest: dict[str, Any], workspace: Path, snapshot: dict[str, Any] | None) -> dict[str, Any]:
    repositories = repo_map(manifest)
    repo_results: list[dict[str, Any]] = []
    for repo_id, repo in repositories.items():
        path = repo_path(workspace, repo)
        item: dict[str, Any] = {"id": repo_id, "path": str(path), "status": "missing", "artifacts": []}
        if not path.is_dir():
            repo_results.append(item)
            continue
        head = git_head(path)
        expected = snapshot.get("repositories", {}).get(repo_id, {}).get("commit") if snapshot else None
        dirty_paths = git_status_paths(path)
        dirty = bool(dirty_paths)
        item.update({"head": head, "branch": git_branch(path), "dirty": dirty, "dirty_paths": dirty_paths, "expected": expected})
        if head is None:
            item["status"] = "not-a-git-repository"
        elif expected and head != expected:
            item["status"] = "dirty-drift" if dirty else "drifted"
        elif dirty:
            item["status"] = "dirty"
        else:
            item["status"] = "pinned" if expected else "clean"
        fetch_url = repo.get("remote", {}).get("fetch_url")
        configured = remote_url(path, "origin")
        push_remote = repo.get("remote", {}).get("push_remote")
        push_url = repo.get("remote", {}).get("push_url")
        configured_push = remote_url(path, push_remote, push=True) if push_remote else None
        if not configured:
            fetch_status = "unavailable"
        elif normalized_url(configured) != normalized_url(fetch_url):
            fetch_status = "mismatch"
        else:
            fetch_status = "configured"
        if push_url is None or push_remote is None:
            push_status = "fetch-only"
        elif not configured_push:
            push_status = "unavailable"
        elif normalized_url(configured_push) != normalized_url(push_url):
            push_status = "mismatch"
        else:
            push_status = "configured"
        item["remote"] = {
            "fetch_remote": "origin",
            "expected": fetch_url,
            "configured": configured,
            "matches": fetch_status == "configured",
            "status": fetch_status,
            "push_remote": push_remote,
            "push_expected": push_url,
            "push_configured": configured_push,
            "push_status": push_status,
            "push_matches": push_status == "configured",
        }
        expected_pins = snapshot.get("repositories", {}).get(repo_id, {}).get("dependency_pins", {}) if snapshot else {}
        pin_results: list[dict[str, Any]] = []
        for pin in repo.get("dependency_pins", []):
            dependency = pin.get("dependency")
            expected_pin = expected_pins.get(dependency, pin.get("revision"))
            if pin.get("kind") == "git-submodule":
                submodule = path / pin.get("path", "")
                actual = git_head(submodule) if submodule.is_dir() else None
                nested_dirty = git_status_paths(submodule) if submodule.is_dir() else []
                pin_result = {"dependency": dependency, "kind": pin.get("kind"), "expected": expected_pin, "actual": actual, "status": "pinned" if actual == expected_pin and not nested_dirty else "drifted", "dirty": bool(nested_dirty), "dirty_paths": nested_dirty}
            else:
                target = workspace / pin.get("workspace_path", "")
                actual = git_head(target) if target.is_dir() else None
                pin_result = {"dependency": dependency, "kind": pin.get("kind"), "expected": expected_pin, "actual": actual, "status": "pinned" if actual == expected_pin else "drifted"}
            pin_results.append(pin_result)
        item["dependency_pins"] = pin_results
        item["artifacts"] = [check_artifact(artifact, path, workspace, repositories) for artifact in repo.get("generated_artifacts", [])]
        repo_results.append(item)
    return {"snapshot": snapshot.get("id") if snapshot else None, "repositories": repo_results}


def status_errors(status: dict[str, Any], *, require_clean: bool = False) -> list[str]:
    errors: list[str] = []
    for repo in status.get("repositories", []):
        if repo.get("status") in {"missing", "not-a-git-repository", "drifted", "dirty-drift"}:
            errors.append(f"{repo.get('id')}: {repo.get('status')}")
        if require_clean and repo.get("dirty"):
            errors.append(f"{repo.get('id')}: worktree is dirty")
        remote = repo.get("remote", {})
        if remote.get("status") in {"unavailable", "mismatch"}:
            errors.append(f"{repo.get('id')}: configured fetch remote is {remote.get('status')}")
        if remote.get("push_status") == "mismatch":
            errors.append(f"{repo.get('id')}: push remote is mismatch")
        for pin in repo.get("dependency_pins", []):
            if pin.get("status") != "pinned":
                errors.append(f"{repo.get('id')}: {pin.get('dependency')} pin is {pin.get('status')}")
            if pin.get("dirty"):
                errors.append(f"{repo.get('id')}: nested {pin.get('dependency')} worktree is dirty")
        for artifact in repo.get("artifacts", []):
            if artifact.get("status") not in {"fresh"}:
                errors.append(f"{repo.get('id')}: artifact {artifact.get('id')} content is {artifact.get('status')}")
            state = artifact.get("provenance_status")
            if state and state not in {"source-current", "content-current"}:
                errors.append(f"{repo.get('id')}: artifact {artifact.get('id')} provenance is {state}")
    return errors


def output(value: Any, json_mode: bool) -> None:
    if json_mode:
        print(json.dumps(value, indent=2, sort_keys=True))
    elif isinstance(value, dict):
        print(value.get("status", "ok"))
    else:
        print(value)


def _current_platform() -> str:
    return {"Darwin": "macos", "Linux": "linux", "Windows": "windows"}.get(platform.system(), platform.system().lower())


def _worker_budget() -> int:
    default = min(4, max(1, os.cpu_count() or 1))
    try:
        configured = int(os.environ.get("ORPHEUS_SUITE_WORKERS", str(default)))
    except ValueError:
        configured = default
    return max(1, min(default, configured))


def command_records(manifest: dict[str, Any], workspace: Path, *, tier: str | None, selected: set[str] | None = None) -> list[dict[str, Any]]:
    current_platform = _current_platform()
    records: list[dict[str, Any]] = []
    _ = _worker_budget()  # one aggregate budget; commands are intentionally serial here
    for repo in manifest.get("repositories", []):
        if selected and repo.get("id") not in selected:
            continue
        for check in repo.get("verification", []):
            if tier == "quick" and check.get("tier") != "quick":
                continue
            if current_platform not in check.get("platforms", []):
                records.append({"repository": repo.get("id"), "id": check.get("id"), "status": "skipped-platform", "required": bool(check.get("required_for")), "platform": current_platform})
                continue
            cwd_value = check.get("cwd")
            if not isinstance(cwd_value, str) or not is_safe_relative(cwd_value):
                records.append({"repository": repo.get("id"), "id": check.get("id"), "status": "unavailable", "error": "unsafe check cwd"})
                continue
            cwd = workspace / cwd_value
            environment = os.environ.copy()
            environment["CMAKE_BUILD_PARALLEL_LEVEL"] = "1"
            result = run_command(check.get("command", []), cwd, env=environment)
            status = "passed" if result.ok else "failed"
            if status == "failed" and result.completed.returncode == 127 and ("not found" in result.output.lower() or "no such file" in result.output.lower()):
                status = "unavailable"
            records.append({
                "repository": repo.get("id"),
                "id": check.get("id"),
                "status": status,
                "required": bool(check.get("required_for")),
                "returncode": result.completed.returncode,
                "cwd": str(cwd),
                "command": check.get("command"),
                "output": result.output,
            })
    return records


def dependency_graph(manifest: dict[str, Any]) -> dict[str, set[str]]:
    graph: dict[str, set[str]] = defaultdict(set)
    for edge in manifest.get("dependencies", []):
        if isinstance(edge, dict) and isinstance(edge.get("from"), str) and isinstance(edge.get("to"), str):
            graph[edge["from"]].add(edge["to"])
    return graph


def affected_ids(manifest: dict[str, Any], changed: Iterable[str]) -> list[str]:
    repository_ids = set(repo_map(manifest))
    changed_list = list(changed)
    unknown = sorted(set(changed_list) - repository_ids)
    if unknown:
        raise SuiteError(f"unknown repositories: {', '.join(unknown)}")
    if len(changed_list) != len(set(changed_list)):
        raise SuiteError("duplicate repository selectors are not allowed")
    graph = dependency_graph(manifest)
    affected = set(changed_list)
    queue = deque(changed_list)
    while queue:
        source = queue.popleft()
        for target in sorted(graph.get(source, set())):
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
    result = run_git(repo_path_value, ["update-ref", ref, "HEAD"])
    if not result.ok:
        raise SuiteError(f"cannot create backup ref {ref}: {result.output.strip()}")
    return ref


def update_manifest_pin(manifest: dict[str, Any], repo_id: str, dependency: str, revision: str) -> bool:
    changed = False
    for repo in manifest.get("repositories", []):
        if repo.get("id") != repo_id:
            continue
        for pin in repo.get("dependency_pins", []):
            if pin.get("dependency") == dependency and pin.get("revision") != revision:
                pin["revision"] = revision
                changed = True
    return changed


def snapshot_dependency_operations(manifest: dict[str, Any], workspace: Path, snapshot: dict[str, Any], repo_ids: Iterable[str]) -> list[dict[str, Any]]:
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
            operations.append({"repository": repo_id, "dependency": pin["dependency"], "kind": pin.get("kind"), "revision": revision, "path": str(path), "relative_path": pin.get("path") or pin.get("workspace_path")})
    return operations


def _probe_advertised(url: str, immutable_ref: str) -> dict[str, Any]:
    key = (normalized_url(url), immutable_ref)
    if key in REMOTE_CACHE:
        return REMOTE_CACHE[key]
    result = run_command(["git", "ls-remote", url, immutable_ref, f"{immutable_ref}^{{}}"], ROOT)
    records: dict[str, str] = {}
    if result.ok:
        for line in result.completed.stdout.splitlines():
            fields = line.split("\t", 1)
            if len(fields) == 2 and is_commit(fields[0]):
                records[fields[1]] = fields[0]
    if not result.ok:
        probe = {"status": "unreachable", "url": url, "ref": immutable_ref, "error": result.output.strip()}
    elif not records:
        probe = {"status": "unadvertised", "url": url, "ref": immutable_ref, "records": records}
    else:
        probe = {"status": "advertised", "url": url, "ref": immutable_ref, "records": records}
    REMOTE_CACHE[key] = probe
    return probe


def _probe_push(url: str) -> dict[str, Any]:
    key = (normalized_url(url), "HEAD")
    if key in REMOTE_CACHE:
        return REMOTE_CACHE[key]
    result = run_command(["git", "ls-remote", url, "HEAD"], ROOT)
    probe = {"status": "reachable" if result.ok else "unreachable", "url": url, "error": result.output.strip() if not result.ok else ""}
    REMOTE_CACHE[key] = probe
    return probe


def _snapshot_revision(snapshot: dict[str, Any] | None, repo_id: str) -> str | None:
    if not snapshot:
        return None
    pin = snapshot.get("repositories", {}).get(repo_id, {})
    return pin.get("commit") if isinstance(pin, dict) else None


def _add_blocker(preflight: dict[str, Any], kind: str, **details: Any) -> None:
    blocker = {"kind": kind, **details}
    key = json.dumps(blocker, sort_keys=True, default=str)
    if key not in preflight.setdefault("_blocker_keys", set()):
        preflight["_blocker_keys"].add(key)
        preflight.setdefault("blockers", []).append(blocker)


def operation_preflight(
    manifest: dict[str, Any],
    workspace: Path,
    snapshot: dict[str, Any] | None,
    *,
    operation: str,
    selected: set[str] | None = None,
    require_push: bool = False,
    check_tier: str | None = None,
    run_checks: bool = False,
    revision_overrides: dict[str, str] | None = None,
) -> dict[str, Any]:
    """Shared read-only gate for status, verification, mutation, and release."""
    repositories = repo_map(manifest)
    revision_overrides = revision_overrides or {}
    preflight: dict[str, Any] = {"operation": operation, "status": "ready", "blockers": [], "repositories": [], "artifacts": [], "remotes": [], "checks": []}
    preflight["_blocker_keys"] = set()
    for repo_id in manifest.get("release_policy", {}).get("order", list(repositories)):
        repo = repositories.get(repo_id)
        if repo is None:
            continue
        path = repo_path(workspace, repo)
        record: dict[str, Any] = {"id": repo_id, "path": str(path)}
        root_state = safe_path_state(workspace, repo["path"], expect_directory=True)
        record["path_status"] = root_state
        if root_state != "present":
            _add_blocker(preflight, root_state, repository=repo_id, path=str(path))
            preflight["repositories"].append(record)
            continue
        record["head"] = git_head(path)
        record["dirty_paths"] = git_status_paths(path)
        record["dirty"] = bool(record["dirty_paths"])
        if record["head"] is None:
            _add_blocker(preflight, "not-a-git-repository", repository=repo_id)
        if record["dirty"]:
            _add_blocker(preflight, "dirty-worktree", repository=repo_id, paths=record["dirty_paths"])
        expected = revision_overrides.get(repo_id, _snapshot_revision(snapshot, repo_id))
        record["expected"] = expected
        if expected and (not is_commit(expected) or not git_revision_exists(path, expected)):
            _add_blocker(preflight, "revision-unavailable", repository=repo_id, revision=expected)
        elif expected and selected and repo_id in selected and record.get("head") != expected:
            _add_blocker(preflight, "drifted-worktree", repository=repo_id, expected=expected, actual=record.get("head"))
        fetch_expected = repo.get("remote", {}).get("fetch_url")
        configured_fetch = remote_url(path, "origin")
        fetch_record = {"expected": fetch_expected, "configured": configured_fetch, "status": "unavailable" if not configured_fetch else ("matches" if normalized_url(configured_fetch) == normalized_url(fetch_expected) else "mismatch")}
        record["fetch_remote"] = fetch_record
        if fetch_record["status"] != "matches":
            _add_blocker(preflight, "remote-" + fetch_record["status"], repository=repo_id, remote="origin")
        snapshot_pin = snapshot.get("repositories", {}).get(repo_id, {}) if snapshot else {}
        if expected and isinstance(snapshot_pin, dict):
            immutable_ref = snapshot_pin.get("immutable_ref")
            if isinstance(immutable_ref, str) and IMMUTABLE_REF_RE.fullmatch(immutable_ref):
                probe = _probe_advertised(str(fetch_expected), immutable_ref)
                remote_record = {"repository": repo_id, "fetch": probe, "immutable_ref": immutable_ref}
                preflight["remotes"].append(remote_record)
                advertised = probe.get("records", {})
                direct = advertised.get(immutable_ref)
                peeled = advertised.get(f"{immutable_ref}^{{}}")
                if probe.get("status") == "unreachable":
                    _add_blocker(preflight, "immutable-ref-unreachable", repository=repo_id, immutable_ref=immutable_ref)
                elif probe.get("status") != "advertised":
                    _add_blocker(preflight, "immutable-ref-unadvertised", repository=repo_id, immutable_ref=immutable_ref)
                elif expected not in {direct, peeled}:
                    _add_blocker(preflight, "immutable-ref-mismatch", repository=repo_id, immutable_ref=immutable_ref, expected=expected, advertised=direct or peeled)
            else:
                _add_blocker(preflight, "immutable-ref-invalid", repository=repo_id)
        for pin in repo.get("dependency_pins", []):
            kind = pin.get("kind")
            if kind == "git-submodule":
                rel = pin.get("path")
                if not isinstance(rel, str) or not is_safe_relative(rel):
                    _add_blocker(preflight, "unsafe-path", repository=repo_id, path=rel)
                    continue
                pin_path = path / rel
            else:
                rel = pin.get("workspace_path")
                if not isinstance(rel, str) or not is_safe_relative(rel):
                    _add_blocker(preflight, "unsafe-path", repository=repo_id, path=rel)
                    continue
                pin_path = workspace / rel
            pin_state = safe_path_state(path if kind == "git-submodule" else workspace, rel, expect_directory=True)
            pin_record = {"repository": repo_id, "dependency": pin.get("dependency"), "path": str(pin_path), "path_status": pin_state, "expected": pin.get("revision")}
            if pin_state != "present":
                _add_blocker(preflight, pin_state, repository=repo_id, dependency=pin.get("dependency"))
            elif not git_revision_exists(pin_path, pin.get("revision", "")):
                _add_blocker(preflight, "dependency-revision-unavailable", repository=repo_id, dependency=pin.get("dependency"), revision=pin.get("revision"))
            nested_paths = git_status_paths(pin_path)
            pin_record["dirty"] = bool(nested_paths)
            pin_record["dirty_paths"] = nested_paths
            if nested_paths:
                _add_blocker(preflight, "dirty-nested-worktree", repository=repo_id, dependency=pin.get("dependency"), paths=nested_paths)
            preflight.setdefault("pins", []).append(pin_record)
        for artifact in repo.get("generated_artifacts", []):
            artifact_record = check_artifact(artifact, path, workspace, repositories)
            artifact_record["repository"] = repo_id
            preflight["artifacts"].append(artifact_record)
            if artifact.get("scope") == "public" and (artifact_record.get("status") != "fresh" or artifact_record.get("provenance_status") not in {"source-current", "content-current"}):
                _add_blocker(preflight, "artifact-not-fresh", repository=repo_id, artifact=artifact.get("id"), content=artifact_record.get("status"), provenance=artifact_record.get("provenance_status"))
        push = repo.get("remote", {})
        push_url = push.get("push_url")
        push_remote_name = push.get("push_remote")
        if push_url is None or push_remote_name is None:
            push_record = {"repository": repo_id, "status": "fetch-only"}
            preflight["remotes"].append(push_record)
            if require_push:
                _add_blocker(preflight, "push-unavailable", repository=repo_id)
        else:
            configured_push = remote_url(path, push_remote_name, push=True)
            if not configured_push:
                push_status = "unavailable"
            elif normalized_url(configured_push) != normalized_url(push_url):
                push_status = "mismatch"
            else:
                push_status = "configured"
            push_record = {"repository": repo_id, "status": push_status, "expected": push_url, "configured": configured_push}
            if push_status == "configured" and require_push:
                push_record["probe"] = _probe_push(str(push_url))
                if push_record["probe"].get("status") != "reachable":
                    push_status = "unreachable"
                    push_record["status"] = push_status
            preflight["remotes"].append(push_record)
            if require_push and push_status != "configured":
                _add_blocker(preflight, "push-" + push_status, repository=repo_id)
        preflight["repositories"].append(record)
    if run_checks:
        preflight["checks"] = command_records(manifest, workspace, tier=check_tier, selected=selected)
        for check in preflight["checks"]:
            if check.get("status") in {"failed", "unavailable", "skipped-platform"} and check.get("required", True):
                _add_blocker(preflight, "check-" + str(check.get("status")), repository=check.get("repository"), check=check.get("id"))
    preflight["status"] = "blocked" if preflight["blockers"] else "ready"
    preflight.pop("_blocker_keys", None)
    return preflight


def _new_journal(workspace: Path, operation: str, manifest_path: Path, items: list[dict[str, Any]]) -> tuple[Path, dict[str, Any]]:
    journal_dir = workspace / ".orpheus-suite" / "journals"
    journal_dir.mkdir(parents=True, exist_ok=True)
    journal_path = journal_dir / f"{operation}-{dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')}-{uuid.uuid4().hex}.json"
    journal = {"version": 1, "kind": "git-operation", "operation": operation, "state": "prepared", "manifest": str(manifest_path), "items": items, "applied": [], "backups": [], "created_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    write_json_atomic(journal_path, journal)
    return journal_path, journal


def _journal_update(path: Path, journal: dict[str, Any], state: str) -> None:
    journal["state"] = state
    write_json_atomic(path, journal)


def _manifest_backup(manifest_path: Path, journal: dict[str, Any], journal_path: Path) -> Path:
    backup = journal_path.with_suffix(".manifest.json")
    shutil.copy2(manifest_path, backup)
    with backup.open("rb") as stream:
        os.fsync(stream.fileno())
    _fsync_parent(backup.parent)
    journal["manifest_backup"] = str(backup)
    _journal_update(journal_path, journal, journal.get("state", "prepared"))
    return backup


def _git_apply_plan(
    *,
    manifest: dict[str, Any],
    manifest_path: Path,
    workspace: Path,
    operation: str,
    plan: list[dict[str, Any]],
    pin_plan: list[dict[str, Any]] | None = None,
    manifest_after: dict[str, Any] | None = None,
) -> dict[str, Any]:
    repositories = repo_map(manifest)
    pin_plan = pin_plan or []
    journal_items: list[dict[str, Any]] = []
    for item in plan:
        journal_items.append({"kind": "repository", "repository": item["repository"], "path": item["path"], "before_head": git_head(Path(item["path"])), "revision": item["revision"]})
    for item in pin_plan:
        journal_items.append({"kind": "pin", "repository": item["repository"], "dependency": item["dependency"], "path": item["path"], "relative_path": item.get("relative_path"), "before_head": git_head(Path(item["path"])), "revision": item["revision"]})
    journal_path, journal = _new_journal(workspace, operation, manifest_path, journal_items)
    result: dict[str, Any] = {"status": "partial", "applied": [], "applied_pins": [], "backups": [], "journals": [str(journal_path)], "recovery": {"command": f"python3 tools/suite.py recover --journal {journal_path} --action complete|restore --json", "refs": []}}
    try:
        if manifest_after is not None:
            _manifest_backup(manifest_path, journal, journal_path)
        seen_backups: set[str] = set()
        stamp = f"{dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')}-{uuid.uuid4().hex}"
        for item in plan:
            repo_id = item["repository"]
            path = Path(item["path"])
            if repo_id in seen_backups:
                continue
            ref = backup_ref(path, repo_id, stamp)
            seen_backups.add(repo_id)
            result["backups"].append(ref)
            result["recovery"]["refs"].append(ref)
            journal["backups"].append({"repository": repo_id, "path": str(path), "ref": ref})
        for item in pin_plan:
            if item.get("kind") != "git-submodule":
                raise SuiteError(f"automatic mutation is not supported for {item.get('kind')} dependency pin {item.get('dependency')}")
            repo_id = item["repository"]
            if repo_id not in seen_backups:
                consumer = repo_path(workspace, repositories[repo_id])
                ref = backup_ref(consumer, repo_id, stamp)
                seen_backups.add(repo_id)
                result["backups"].append(ref)
                result["recovery"]["refs"].append(ref)
                journal["backups"].append({"repository": repo_id, "path": str(consumer), "ref": ref})
        _journal_update(journal_path, journal, "old-moved")
        for item in plan:
            path = Path(item["path"])
            run_git(path, ["switch", "--detach", item["revision"]], check=True)
            result["applied"].append(item["repository"])
            journal["applied"].append({"kind": "repository", "repository": item["repository"], "after_head": git_head(path)})
            _journal_update(journal_path, journal, "old-moved")
        for item in pin_plan:
            if item.get("kind") != "git-submodule":
                continue
            pin_path = Path(item["path"])
            consumer = repo_path(workspace, repositories[item["repository"]])
            run_git(pin_path, ["switch", "--detach", item["revision"]], check=True)
            run_git(consumer, ["add", item["relative_path"]], check=True)
            result["applied_pins"].append(f"{item['repository']}:{item['dependency']}")
            journal["applied"].append({"kind": "pin", "repository": item["repository"], "dependency": item["dependency"], "after_head": git_head(pin_path)})
            _journal_update(journal_path, journal, "old-moved")
        _journal_update(journal_path, journal, "new-promoted")
        if manifest_after is not None:
            write_json_atomic(manifest_path, manifest_after)
        _journal_update(journal_path, journal, "committed")
        result["status"] = "applied"
        return result
    except Exception as exc:
        journal["error"] = str(exc)
        _journal_update(journal_path, journal, journal.get("state", "prepared"))
        result["error"] = str(exc)
        result["status"] = "partial"
        raise PartialApply(result) from exc


def _planned_result(base: dict[str, Any], preflight: dict[str, Any]) -> tuple[dict[str, Any], int]:
    result = {**base, "status": "blocked" if preflight.get("blockers") else "planned", "blockers": preflight.get("blockers", []), "preflight": preflight, "applied": []}
    return result, 1 if preflight.get("blockers") else 0


def cmd_validate(args: argparse.Namespace) -> int:
    manifest_path = Path(args.manifest).expanduser().absolute() if args.manifest else DEFAULT_MANIFEST
    schema_path = _schema_path(manifest_path)
    errors: list[dict[str, Any]] = []
    try:
        value = load_json_value(manifest_path)
    except SuiteError as exc:
        errors.append({"kind": "input", "path": "$", "message": str(exc)})
        value = None
    errors.extend(schema_validation_errors(value, schema_path))
    errors.extend(_business_error(message) for message in validate_manifest(value))
    result = {"manifest": str(manifest_path), "schema": str(schema_path), "valid": not errors, "errors": errors}
    output(result, args.json)
    return 0 if not errors else 1


def cmd_status(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, args.channel)
    status = snapshot_status(manifest, workspace, snapshot)
    preflight = operation_preflight(manifest, workspace, snapshot, operation="status")
    errors = status_errors(status) + [f"{item['kind']}: {item.get('repository', '')}" for item in preflight.get("blockers", [])]
    result = {**status, "workspace": str(workspace), "channel": args.channel or manifest.get("release_channel"), "preflight": preflight, "errors": errors, "status": "blocked" if errors else "healthy"}
    output(result, args.json)
    return 1 if errors else 0


def cmd_doctor(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, args.snapshot, args.channel)
    status = snapshot_status(manifest, workspace, snapshot)
    selected = set(args.repositories) if args.repositories else None
    if selected and not selected.issubset(repo_map(manifest)):
        raise SuiteError("unknown repositories: " + ", ".join(sorted(selected - set(repo_map(manifest)))))
    checks = command_records(manifest, workspace, tier="quick", selected=selected) if args.checks else []
    preflight = operation_preflight(manifest, workspace, snapshot, operation="doctor", selected=selected)
    errors = status_errors(status, require_clean=args.require_clean)
    errors.extend(f"{item['kind']}: {item.get('repository', '')}" for item in preflight.get("blockers", []))
    errors.extend(f"{item.get('repository')}: check {item.get('id')} {item.get('status')}" for item in checks if item.get("status") in {"failed", "unavailable", "skipped-platform"})
    result = {**status, "workspace": str(workspace), "preflight": preflight, "checks": checks, "errors": errors, "status": "blocked" if errors else "healthy"}
    output(result, args.json)
    return 1 if errors else 0


def cmd_verify(args: argparse.Namespace) -> int:
    _, manifest, workspace = manifest_context(args)
    selected = set(args.repositories) if args.repositories else None
    if selected and not selected.issubset(repo_map(manifest)):
        raise SuiteError("unknown repositories: " + ", ".join(sorted(selected - set(repo_map(manifest)))))
    snapshot = selected_snapshot(manifest, getattr(args, "snapshot", None), getattr(args, "channel", None))
    preflight = operation_preflight(manifest, workspace, snapshot, operation="verify", selected=selected)
    records = command_records(manifest, workspace, tier=None if args.full else "quick", selected=selected)
    failures = [record for record in records if record.get("status") in {"failed", "unavailable", "skipped-platform"}]
    blockers = preflight.get("blockers", []) + [{"kind": "check-" + str(record.get("status")), "repository": record.get("repository"), "check": record.get("id")} for record in failures]
    result = {"status": "blocked" if blockers else ("failed" if failures else "passed"), "workspace": str(workspace), "tier": "full" if args.full else "quick", "preflight": preflight, "checks": records, "failures": failures, "blockers": blockers}
    output(result, args.json)
    return 1 if blockers else 0


def cmd_affected(args: argparse.Namespace) -> int:
    _, manifest, _ = manifest_context(args)
    affected = affected_ids(manifest, args.repositories)
    result = {"status": "planned", "changed": args.repositories, "affected": affected, "merge_order": manifest["release_policy"]["order"]}
    output(result, args.json)
    return 0


def _update_plans(manifest: dict[str, Any], workspace: Path, source: str, revision: str, affected: bool) -> tuple[list[str], list[dict[str, Any]], list[dict[str, Any]]]:
    repositories = repo_map(manifest)
    closure = affected_ids(manifest, [source])
    selected = closure if affected else [source] + [repo_id for repo_id in closure if repo_id != source and any(edge.get("from") == source and edge.get("to") == repo_id for edge in manifest.get("dependencies", []))]
    selected = [repo_id for repo_id in manifest["release_policy"]["order"] if repo_id in selected]
    plan = [{"repository": source, "revision": revision, "path": str(repo_path(workspace, repositories[source])), "action": "update-repository"}]
    pin_plan: list[dict[str, Any]] = []
    for repo_id in selected:
        if repo_id == source:
            continue
        for pin in repositories[repo_id].get("dependency_pins", []):
            if pin.get("dependency") != source:
                continue
            pin_plan.append({"repository": repo_id, "dependency": source, "kind": pin.get("kind"), "revision": revision, "path": str((repo_path(workspace, repositories[repo_id]) / pin["path"]) if pin.get("kind") == "git-submodule" else workspace / pin.get("workspace_path", "")), "relative_path": pin.get("path") or pin.get("workspace_path"), "action": "update-exact-pin" if pin.get("kind") == "git-submodule" else "manual-source-or-package-update"})
    return selected, plan, pin_plan


def cmd_update(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    repositories = repo_map(manifest)
    source = args.repository
    if source not in repositories:
        raise SuiteError(f"unknown repository: {source}")
    if not is_commit(args.revision):
        raise SuiteError("--revision must be a 40-character lowercase commit")
    selected, plan, pin_plan = _update_plans(manifest, workspace, source, args.revision, args.affected)
    preflight = operation_preflight(manifest, workspace, selected_snapshot(manifest, None, None), operation="update", selected=set(selected), revision_overrides={source: args.revision})
    if not git_revision_exists(repo_path(workspace, repositories[source]), args.revision):
        _add_blocker(preflight, "revision-unavailable", repository=source, revision=args.revision)
    if any(item.get("kind") != "git-submodule" for item in pin_plan):
        _add_blocker(preflight, "manual-pin-update-required", repository=source)
    base = {"operation": "update", "source": source, "revision": args.revision, "affected": selected, "plan": plan, "pin_plan": pin_plan, "backups": [], "journals": []}
    if not args.apply:
        result, code = _planned_result(base, preflight)
        output(result, args.json)
        return code
    if not args.yes:
        raise SuiteError("update --apply requires --yes; the default is a dry-run")
    if preflight.get("blockers"):
        result, _ = _planned_result(base, preflight)
        result["status"] = "blocked"
        output(result, args.json)
        return 1
    manifest_after = json.loads(json.dumps(manifest))
    for item in pin_plan:
        if item.get("kind") == "git-submodule":
            update_manifest_pin(manifest_after, item["repository"], source, args.revision)
    try:
        result = _git_apply_plan(manifest=manifest, manifest_path=manifest_path, workspace=workspace, operation="update", plan=plan, pin_plan=pin_plan, manifest_after=manifest_after)
    except PartialApply as exc:
        output(exc.envelope, args.json)
        return 2
    output(result, args.json)
    return 0


def _sync_like(args: argparse.Namespace, operation: Literal["sync", "rollback"]) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    snapshot = selected_snapshot(manifest, getattr(args, "snapshot", None), getattr(args, "channel", None)) if operation == "sync" else selected_snapshot(manifest, args.snapshot, None)
    if snapshot is None:
        raise SuiteError(f"{operation} requires a pinned snapshot")
    repositories = repo_map(manifest)
    requested_ids = list(getattr(args, "repositories", []) or [])
    if operation == "rollback":
        requested_ids = list(manifest["release_policy"]["order"])
    if getattr(args, "affected", False):
        if not requested_ids:
            raise SuiteError("--affected requires one or more repository IDs")
        target_ids = affected_ids(manifest, requested_ids)
    else:
        target_ids = requested_ids or list(manifest["release_policy"]["order"])
    unknown = sorted(set(target_ids) - set(repositories))
    if unknown:
        raise SuiteError(f"unknown repositories: {', '.join(unknown)}")
    plan = [{"repository": repo_id, "revision": snapshot["repositories"][repo_id]["commit"], "path": str(repo_path(workspace, repositories[repo_id])), "action": "checkout-exact-revision"} for repo_id in target_ids]
    pin_plan = snapshot_dependency_operations(manifest, workspace, snapshot, target_ids)
    preflight = operation_preflight(manifest, workspace, snapshot, operation=operation, selected=set(target_ids))
    for item in pin_plan:
        if item.get("kind") != "git-submodule":
            _add_blocker(preflight, "manual-pin-update-required", repository=item.get("repository"), dependency=item.get("dependency"))
    base = {"operation": operation, "snapshot": snapshot["id"], "plan": plan, "pin_plan": pin_plan, "backups": [], "journals": []}
    if not getattr(args, "apply", False):
        result, code = _planned_result(base, preflight)
        output(result, args.json)
        return code
    if not getattr(args, "yes", False):
        raise SuiteError(f"{operation} --apply requires --yes; the default is a dry-run")
    if preflight.get("blockers"):
        result, _ = _planned_result(base, preflight)
        result["status"] = "blocked"
        output(result, args.json)
        return 1
    try:
        result = _git_apply_plan(manifest=manifest, manifest_path=manifest_path, workspace=workspace, operation=operation, plan=plan, pin_plan=pin_plan)
    except PartialApply as exc:
        output(exc.envelope, args.json)
        return 2
    output(result, args.json)
    return 0


def cmd_sync(args: argparse.Namespace) -> int:
    return _sync_like(args, "sync")


def cmd_rollback(args: argparse.Namespace) -> int:
    return _sync_like(args, "rollback")


def acceptance_value(args: argparse.Namespace) -> dict[str, Any]:
    if not args.acceptance:
        return {"status": "pending", "records": []}
    value = load_json(Path(args.acceptance).expanduser().absolute())
    if value.get("status") not in {"pending", "passed", "failed"} or not isinstance(value.get("records"), list):
        raise SuiteError("acceptance file must contain {status: pending|passed|failed, records: []}")
    return value


def _release_preflight(manifest: dict[str, Any], workspace: Path, snapshot: dict[str, Any], selected: set[str] | None) -> dict[str, Any]:
    return operation_preflight(manifest, workspace, snapshot, operation="release", selected=selected, require_push=True, check_tier=None, run_checks=True)


def cmd_release(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    action = args.release_action
    source_snapshot = selected_snapshot(manifest, args.from_snapshot, args.channel)
    if source_snapshot is None:
        raise SuiteError("release requires a source snapshot via --from-snapshot or a pinned channel")
    selected = set(args.repositories) if args.repositories else None
    if selected and not selected.issubset(repo_map(manifest)):
        raise SuiteError("unknown repositories: " + ", ".join(sorted(selected - set(repo_map(manifest)))))
    acceptance = acceptance_value(args)
    preflight = _release_preflight(manifest, workspace, source_snapshot, selected)
    check_records = preflight.get("checks", [])
    blockers = list(preflight.get("blockers", []))
    if action == "candidate":
        if not args.version or not SEMVER_RE.fullmatch(args.version):
            raise SuiteError("release candidate requires --version X.Y.Z")
        if acceptance.get("status") != "passed":
            blockers.append({"kind": "human-acceptance-not-passed"})
        snapshot_id = args.snapshot_id or f"candidate-{args.version.replace('.', '-')}-{dt.date.today().isoformat().replace('-', '')}"
        if not ID_RE.fullmatch(snapshot_id):
            raise SuiteError("--snapshot-id must be a lowercase suite snapshot id")
        verification = {"status": "passed" if not blockers else "blocked", "evidence": [f"source snapshot: {source_snapshot['id']}"] + [f"{item.get('repository')}/{item.get('id')}: {item.get('status')}" for item in check_records]}
        base = {"operation": "release candidate", "snapshot_id": snapshot_id, "source_snapshot": source_snapshot["id"], "verification": verification, "human_acceptance": acceptance, "check_records": check_records, "backups": [], "journals": []}
        if not args.apply:
            result = {**base, "status": "blocked" if blockers else "planned", "blockers": blockers, "applied": []}
            output(result, args.json)
            return 1 if blockers else 0
        if not args.yes:
            raise SuiteError("release candidate --apply requires --yes")
        if blockers:
            result = {**base, "status": "blocked", "blockers": blockers, "applied": []}
            output(result, args.json)
            return 1
        if snapshot_id in manifest["snapshots"]:
            raise SuiteError(f"snapshot already exists: {snapshot_id}")
        require_manifest_owner_clean(manifest_path)
        manifest_after = json.loads(json.dumps(manifest))
        snapshot = capture_snapshot(manifest, workspace, snapshot_id, args.version, "candidate", "candidate", verification, acceptance)
        manifest_after["snapshots"][snapshot_id] = snapshot
        manifest_after["channels"]["candidate"]["snapshot_id"] = snapshot_id
        manifest_after["suite_version"] = args.version
        journal_path, journal = _new_journal(workspace, "release-candidate", manifest_path, [])
        result = {**base, "status": "partial", "applied": [], "journals": [str(journal_path)], "recovery": {"command": f"python3 tools/suite.py recover --journal {journal_path} --action complete|restore --json", "refs": []}}
        try:
            _manifest_backup(manifest_path, journal, journal_path)
            _journal_update(journal_path, journal, "old-moved")
            write_json_atomic(manifest_path, manifest_after)
            _journal_update(journal_path, journal, "new-promoted")
            _journal_update(journal_path, journal, "committed")
            result.update({"status": "applied", "applied": ["manifest"]})
        except Exception as exc:
            journal["error"] = str(exc)
            _journal_update(journal_path, journal, journal.get("state", "prepared"))
            result["error"] = str(exc)
            output(result, args.json)
            return 2
        output(result, args.json)
        return 0
    candidate_id = args.from_snapshot or manifest.get("channels", {}).get("candidate", {}).get("snapshot_id")
    candidate = manifest.get("snapshots", {}).get(candidate_id) if candidate_id else None
    if not isinstance(candidate, dict) or candidate.get("state") != "candidate":
        raise SuiteError("stable promotion requires a candidate snapshot")
    if candidate.get("verification", {}).get("status") != "passed":
        blockers.append({"kind": "candidate-verification-not-passed"})
    if candidate.get("human_acceptance", {}).get("status") != "passed":
        blockers.append({"kind": "candidate-human-acceptance-not-passed"})
    base = {"operation": "release stable", "snapshot_id": candidate["id"], "check_records": check_records, "backups": [], "journals": []}
    if not args.apply:
        result = {**base, "status": "blocked" if blockers else "planned", "blockers": blockers, "applied": []}
        output(result, args.json)
        return 1 if blockers else 0
    if not args.yes:
        raise SuiteError("release stable --apply requires --yes")
    if blockers:
        result = {**base, "status": "blocked", "blockers": blockers, "applied": []}
        output(result, args.json)
        return 1
    require_manifest_owner_clean(manifest_path)
    manifest_after = json.loads(json.dumps(manifest))
    manifest_after["snapshots"][candidate["id"]]["state"] = "stable"
    manifest_after["snapshots"][candidate["id"]]["channel"] = "stable"
    manifest_after["channels"]["stable"]["snapshot_id"] = candidate["id"]
    manifest_after["release_channel"] = "stable"
    journal_path, journal = _new_journal(workspace, "release-stable", manifest_path, [])
    result = {**base, "status": "partial", "applied": [], "journals": [str(journal_path)], "recovery": {"command": f"python3 tools/suite.py recover --journal {journal_path} --action complete|restore --json", "refs": []}}
    try:
        _manifest_backup(manifest_path, journal, journal_path)
        _journal_update(journal_path, journal, "old-moved")
        write_json_atomic(manifest_path, manifest_after)
        _journal_update(journal_path, journal, "new-promoted")
        _journal_update(journal_path, journal, "committed")
        result.update({"status": "applied", "applied": ["manifest"]})
    except Exception as exc:
        journal["error"] = str(exc)
        _journal_update(journal_path, journal, journal.get("state", "prepared"))
        result["error"] = str(exc)
        output(result, args.json)
        return 2
    output(result, args.json)
    return 0


def capture_snapshot(manifest: dict[str, Any], workspace: Path, snapshot_id: str, suite_version: str, channel: str, state: str, verification: dict[str, Any], acceptance: dict[str, Any]) -> dict[str, Any]:
    repositories: dict[str, Any] = {}
    for repo in manifest["repositories"]:
        path = repo_path(workspace, repo)
        head = git_head(path)
        if not head or not is_commit(head):
            raise SuiteError(f"cannot capture {repo['id']}: no valid HEAD")
        pins: dict[str, str] = {}
        for pin in repo.get("dependency_pins", []):
            child = path / pin["path"] if pin.get("kind") == "git-submodule" else workspace / pin.get("workspace_path", "")
            actual = git_head(child)
            if actual and is_commit(actual):
                pins[pin["dependency"]] = actual
        tag = f"suite-{snapshot_id}-{repo['id']}"
        repositories[repo["id"]] = {"commit": head, "branch": git_branch(path) or "detached", "tag": tag, "immutable_ref": f"refs/tags/{tag}", **({"dependency_pins": pins} if pins else {})}
    return {"id": snapshot_id, "suite_version": suite_version, "channel": channel, "state": state, "immutable": True, "observed_at": dt.date.today().isoformat(), "repositories": repositories, "verification": verification, "human_acceptance": acceptance}


def github_repo_from_url(url: str) -> str:
    parsed = urllib.parse.urlsplit(url)
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
    request = urllib.request.Request(f"https://api.github.com/repos/{repo_name}/pulls", data=payload, headers={"Accept": "application/vnd.github+json", "Authorization": f"Bearer {token}", "X-GitHub-Api-Version": "2022-11-28", "Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            value = json.loads(response.read().decode("utf-8"))
    except (urllib.error.HTTPError, urllib.error.URLError, json.JSONDecodeError) as exc:
        raise SuiteError(f"GitHub PR creation failed for {repo_name}: {exc}") from exc
    if not isinstance(value, dict) or not value.get("html_url"):
        raise SuiteError(f"GitHub PR response did not contain html_url for {repo_name}")
    return str(value["html_url"])


def cmd_coordinate(args: argparse.Namespace) -> int:
    manifest_path, manifest, workspace = manifest_context(args)
    repositories = repo_map(manifest)
    source = args.repository
    if source not in repositories:
        raise SuiteError(f"unknown repository: {source}")
    if not is_commit(args.revision):
        raise SuiteError("--revision must be a 40-character lowercase commit")
    if not CHANGE_ID_RE.fullmatch(args.change_id):
        raise SuiteError("--change-id must match ORP-SUITE-YYYYMMDD-NNN")
    affected = affected_ids(manifest, [source])
    downstream = [repo_id for repo_id in affected if repo_id != source]
    plans: list[dict[str, Any]] = []
    for repo_id in downstream:
        for pin in repositories[repo_id].get("dependency_pins", []):
            if pin.get("dependency") == source:
                plans.append({"repository": repo_id, "kind": pin.get("kind"), "path": pin.get("path") or pin.get("workspace_path"), "action": "create-dependent-pr" if pin.get("kind") == "git-submodule" else "manual-dependent-change"})
    preflight = operation_preflight(manifest, workspace, selected=set(downstream), operation="coordinate", require_push=True)
    if not git_revision_exists(repo_path(workspace, repositories[source]), args.revision):
        _add_blocker(preflight, "revision-unavailable", repository=source, revision=args.revision)
    if any(plan["action"] == "manual-dependent-change" for plan in plans):
        _add_blocker(preflight, "manual-dependent-change", repository=source)
    base = {"operation": "coordinate", "change_id": args.change_id, "source": source, "revision": args.revision, "affected": affected, "plans": plans, "pull_requests": [], "backups": [], "journals": []}
    if not args.apply:
        result, code = _planned_result(base, preflight)
        result["pull_requests"] = []
        output(result, args.json)
        return code
    if not args.yes:
        raise SuiteError("coordinate --apply requires --yes")
    token = os.environ.get(args.token_env)
    if not token:
        raise SuiteError(f"coordinate --apply requires {args.token_env} for GitHub PR creation")
    if preflight.get("blockers"):
        result, _ = _planned_result(base, preflight)
        result["status"] = "blocked"
        output(result, args.json)
        return 1
    journal_path, journal = _new_journal(workspace, "coordinate", manifest_path, [])
    result = {**base, "status": "partial", "journals": [str(journal_path)], "recovery": {"command": f"python3 tools/suite.py recover --journal {journal_path} --action complete|restore --json", "refs": []}}
    worktrees: list[tuple[Path, Path]] = []
    try:
        for plan in plans:
            consumer = repo_path(workspace, repositories[plan["repository"]])
            repo_name = github_repo_from_url(repositories[plan["repository"]]["remote"]["push_url"])
            branch = f"{manifest['coordination']['branch_prefix']}{args.change_id.lower()}-{plan['repository']}"
            safe_relative_path(branch)
            temporary = Path(tempfile.mkdtemp(prefix=f"orpheus-suite-{plan['repository']}-"))
            worktree = temporary / plan["repository"]
            run_git(consumer, ["worktree", "add", "-b", branch, str(worktree), repositories[plan["repository"]]["default_branch"]], check=True)
            worktrees.append((consumer, worktree))
            run_git(worktree, ["submodule", "update", "--init", "--recursive", plan["path"]], check=True)
            child = worktree / plan["path"]
            if not git_revision_exists(child, args.revision):
                raise SuiteError(f"revision {args.revision} is not available for {plan['repository']}")
            run_git(child, ["switch", "--detach", args.revision], check=True)
            run_git(worktree, ["add", plan["path"]], check=True)
            run_git(worktree, ["commit", "-m", f"suite: pin {source} for {args.change_id}"], check=True)
            push_remote = repositories[plan["repository"]]["remote"].get("push_remote", "origin")
            run_git(worktree, ["push", push_remote, f"HEAD:{branch}"], check=True)
            body = "\n".join([f"Suite change: `{args.change_id}`", f"Upstream repository: `{source}`", f"Upstream revision: `{args.revision}`", "", "Generated by tools/suite.py coordinate."])
            url = github_create_pull(repo_name, token, f"{manifest['coordination']['pr_title_prefix']} pin {source} ({args.change_id})", branch, repositories[plan["repository"]]["default_branch"], body)
            result["pull_requests"].append({"repository": plan["repository"], "branch": branch, "url": url})
            journal["applied"].append({"repository": plan["repository"], "branch": branch, "url": url})
            _journal_update(journal_path, journal, "new-promoted")
        _journal_update(journal_path, journal, "committed")
        result["status"] = "applied"
    except Exception as exc:
        journal["error"] = str(exc)
        journal["already_pushed"] = result["pull_requests"]
        _journal_update(journal_path, journal, journal.get("state", "prepared"))
        result["status"] = "partial"
        result["error"] = str(exc)
        output(result, args.json)
        return 2
    finally:
        for consumer, worktree in reversed(worktrees):
            run_git(consumer, ["worktree", "remove", "--force", str(worktree)])
            shutil.rmtree(worktree.parent, ignore_errors=True)
    output(result, args.json)
    return 0


def recover_git_journal(journal_path: Path, action: Literal["complete", "restore"]) -> dict[str, Any]:
    journal = load_json(journal_path)
    if journal.get("kind") != "git-operation":
        return recover_provenance_journal(journal_path, action)
    state = journal.get("state")
    if state == "committed" and action == "restore":
        raise SuiteError("recovery conflict: committed journal cannot be restored")
    if state == "committed":
        return {"status": "committed", "journal": str(journal_path), "state": state}
    if action == "complete":
        for item in journal.get("items", []):
            path = Path(item["path"])
            if not git_revision_exists(path, item["revision"]):
                raise SuiteError(f"recovery conflict: expected revision is unavailable at {path}")
            if item.get("kind") == "repository" and git_head(path) != item["revision"]:
                run_git(path, ["switch", "--detach", item["revision"]], check=True)
        journal["state"] = "committed"
        write_json_atomic(journal_path, journal)
        return {"status": "committed", "journal": str(journal_path), "state": "committed"}
    # Restore is explicitly destructive only because the operator requested it.
    for applied in journal.get("applied", []):
        if applied.get("kind") == "repository":
            path = Path(next(item["path"] for item in journal.get("items", []) if item.get("kind") == "repository" and item.get("repository") == applied.get("repository")))
            if git_head(path) != applied.get("after_head") or git_dirty(path):
                raise SuiteError(f"recovery conflict: {path} changed after the interrupted apply")
    for backup in journal.get("backups", []):
        path = Path(backup["path"])
        ref = backup["ref"]
        if not git_revision_exists(path, ref):
            raise SuiteError(f"recovery conflict: backup ref is unavailable: {ref}")
        run_git(path, ["switch", "--detach", ref], check=True)
    manifest_backup = journal.get("manifest_backup")
    if manifest_backup:
        manifest_path = Path(journal["manifest"])
        if manifest_path.exists() and hash_file(manifest_path) != hash_file(Path(manifest_backup)):
            raise SuiteError("recovery conflict: manifest changed after the interrupted apply")
        shutil.copy2(manifest_backup, manifest_path)
        _fsync_parent(manifest_path.parent)
    journal["state"] = "restored"
    write_json_atomic(journal_path, journal)
    return {"status": "restored", "journal": str(journal_path), "state": "restored", "backups": journal.get("backups", [])}


def cmd_recover(args: argparse.Namespace) -> int:
    result = recover_git_journal(Path(args.journal).expanduser().absolute(), args.action)
    output(result, args.json)
    return 0


def add_common(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--manifest", help="path to the suite manifest")
    parser.add_argument("--workspace-root", help="workspace containing the declared repository paths")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")


def parser_for() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="suite", description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate")
    add_common(validate)
    status = subparsers.add_parser("status")
    add_common(status)
    status.add_argument("--channel", choices=["development", "candidate", "stable"])
    status.add_argument("--snapshot")
    doctor = subparsers.add_parser("doctor")
    add_common(doctor)
    doctor.add_argument("--channel", choices=["development", "candidate", "stable"])
    doctor.add_argument("--snapshot")
    doctor.add_argument("--checks", action="store_true")
    doctor.add_argument("--require-clean", action="store_true")
    doctor.add_argument("--repositories", nargs="*")
    verify = subparsers.add_parser("verify")
    add_common(verify)
    verify.add_argument("--full", action="store_true")
    verify.add_argument("--repositories", nargs="*")
    verify.add_argument("--snapshot")
    verify.add_argument("--channel", choices=["development", "candidate", "stable"])
    affected = subparsers.add_parser("affected")
    add_common(affected)
    affected.add_argument("repositories", nargs="+")
    update = subparsers.add_parser("update")
    add_common(update)
    update.add_argument("repository")
    update.add_argument("--revision", required=True)
    update.add_argument("--affected", action="store_true")
    update.add_argument("--apply", action="store_true")
    update.add_argument("--yes", action="store_true")
    sync = subparsers.add_parser("sync")
    add_common(sync)
    sync.add_argument("--snapshot")
    sync.add_argument("--channel", choices=["development", "candidate", "stable"])
    sync.add_argument("--repositories", nargs="*")
    sync.add_argument("--affected", action="store_true")
    sync.add_argument("--apply", action="store_true")
    sync.add_argument("--yes", action="store_true")
    release = subparsers.add_parser("release")
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
    rollback = subparsers.add_parser("rollback")
    add_common(rollback)
    rollback.add_argument("snapshot")
    rollback.add_argument("--apply", action="store_true")
    rollback.add_argument("--yes", action="store_true")
    coordinate = subparsers.add_parser("coordinate")
    add_common(coordinate)
    coordinate.add_argument("repository")
    coordinate.add_argument("--revision", required=True)
    coordinate.add_argument("--change-id", required=True)
    coordinate.add_argument("--token-env", default="ORPHEUS_SUITE_GITHUB_TOKEN")
    coordinate.add_argument("--apply", action="store_true")
    coordinate.add_argument("--yes", action="store_true")
    recover = subparsers.add_parser("recover")
    recover.add_argument("--journal", required=True)
    recover.add_argument("--action", choices=["complete", "restore"], required=True)
    recover.add_argument("--json", action="store_true")
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
        if args.command == "release":
            return cmd_release(args)
        if args.command == "rollback":
            return cmd_rollback(args)
        if args.command == "coordinate":
            return cmd_coordinate(args)
        if args.command == "recover":
            return cmd_recover(args)
    except PartialApply as exc:
        output(exc.envelope, getattr(args, "json", False))
        return 2
    except SuiteError as exc:
        if getattr(args, "json", False):
            print(json.dumps({"status": "error", "error": str(exc)}, indent=2, sort_keys=True))
        else:
            print(f"suite: error: {exc}", file=sys.stderr)
        return 2
    parser.error(f"unhandled command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
