"""Safe, source-aware generated-artifact provenance primitives.

The package intentionally depends only on the Python standard library.  The
suite coordinator uses these functions as a locked tool boundary so artifact
inventory and journal recovery do not import the SDK checkout or ambient
``PYTHONPATH`` state.
"""

from __future__ import annotations

import hashlib
import errno
import json
import os
import stat
from pathlib import Path, PurePosixPath
from typing import Any, Literal

__version__ = "1.0.0"
HASH_ALGORITHM = "sha256-path-nul-content-v1"


class ProvenanceError(RuntimeError):
    """A safe-path, inventory, or journal contract violation."""


class SafePathUnsupported(ProvenanceError):
    """The host cannot provide the no-follow descriptor API we require."""


class UnsafePath(ProvenanceError):
    """A symlink, traversal, or non-regular artifact component was found."""


def _supports_descriptor_safety() -> bool:
    required = ("O_NOFOLLOW", "O_DIRECTORY")
    if any(not hasattr(os, name) for name in required):
        return False
    return getattr(os, "open", None) in getattr(os, "supports_dir_fd", set())


def ensure_descriptor_safety() -> None:
    if not _supports_descriptor_safety():
        raise SafePathUnsupported("safe-path-unsupported: no-follow descriptor traversal is unavailable")


def safe_relative_path(value: str) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise UnsafePath("unsafe-path: path must be a non-empty string without NUL")
    if "\\" in value:
        raise UnsafePath(f"unsafe-path: backslash is not a portable separator: {value}")
    path = PurePosixPath(value)
    if path.is_absolute() or value.startswith("/") or any(part in {"", ".", ".."} for part in path.parts):
        raise UnsafePath(f"unsafe-path: expected a safe relative path: {value}")
    return path.as_posix()


def _open_directory(path: Path, *, dir_fd: int | None = None) -> int:
    ensure_descriptor_safety()
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    try:
        return os.open(str(path), flags) if dir_fd is None else os.open(str(path), flags, dir_fd=dir_fd)
    except OSError as exc:
        if exc.errno in {errno.ELOOP, errno.ENOTDIR}:
            raise UnsafePath(f"symlink-escape: directory component is not a real directory: {path}") from exc
        raise


def _open_child(fd: int, name: str, flags: int) -> int:
    ensure_descriptor_safety()
    try:
        return os.open(name, flags | os.O_NOFOLLOW, dir_fd=fd)
    except OSError as exc:
        if exc.errno == errno.ELOOP:
            raise UnsafePath(f"symlink-escape: component is a symlink: {name}") from exc
        raise


def _relative_parts(relative: str) -> list[str]:
    return safe_relative_path(relative).split("/")


def open_relative(root: Path, relative: str, *, flags: int = os.O_RDONLY) -> int:
    """Open a relative path beneath *root* without following any component."""
    parts = _relative_parts(relative)
    root_fd = _open_directory(root)
    current_fd = root_fd
    try:
        for part in parts[:-1]:
            next_fd = _open_child(current_fd, part, os.O_RDONLY | os.O_DIRECTORY)
            if current_fd != root_fd:
                os.close(current_fd)
            current_fd = next_fd
        fd = _open_child(current_fd, parts[-1], flags)
        if current_fd != root_fd:
            os.close(current_fd)
        os.close(root_fd)
        return fd
    except Exception:
        if current_fd != root_fd:
            os.close(current_fd)
        os.close(root_fd)
        raise


def _read_fd(fd: int) -> bytes:
    chunks: list[bytes] = []
    while True:
        chunk = os.read(fd, 1024 * 1024)
        if not chunk:
            return b"".join(chunks)
        chunks.append(chunk)


def _walk_directory(fd: int, prefix: str, excluded: set[str], inventory: list[str], digest: "hashlib._Hash") -> None:
    try:
        names = sorted(os.listdir(fd))
    except TypeError as exc:
        raise SafePathUnsupported("safe-path-unsupported: descriptor directory listing is unavailable") from exc
    for name in names:
        relative = f"{prefix}/{name}" if prefix else name
        # All manifest exclusions are canonical relative paths.  A directory
        # exclusion is also allowed to prune its descendants.
        if relative in excluded or any(relative.startswith(item.rstrip("/") + "/") for item in excluded):
            continue
        child_fd = _open_child(fd, name, os.O_RDONLY)
        try:
            info = os.fstat(child_fd)
            mode = info.st_mode
            if stat.S_ISDIR(mode):
                _walk_directory(child_fd, relative, excluded, inventory, digest)
            elif stat.S_ISREG(mode):
                inventory.append(relative)
                digest.update(relative.encode("utf-8"))
                digest.update(b"\0")
                while True:
                    chunk = os.read(child_fd, 1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
                digest.update(b"\0")
            else:
                raise UnsafePath(f"unsafe-path: artifact component is not a regular file or directory: {relative}")
        finally:
            os.close(child_fd)


def inventory_tree(root: Path, excluded: set[str]) -> tuple[list[str], str]:
    """Return the canonical regular-file inventory and v1 tree digest.

    Traversal is descriptor-relative and rejects symlinks and special files.
    ``excluded`` contains canonical relative POSIX paths and may name a
    directory to prune.
    """
    root = Path(root)
    canonical_excluded = {safe_relative_path(value) for value in excluded}
    root_fd = _open_directory(root)
    try:
        inventory: list[str] = []
        digest = hashlib.sha256()
        _walk_directory(root_fd, "", canonical_excluded, inventory, digest)
        return inventory, digest.hexdigest()
    finally:
        os.close(root_fd)


def _manifest_source(manifest: dict[str, Any]) -> dict[str, Any]:
    source = manifest.get("source")
    if isinstance(source, dict):
        return source
    return manifest


def _manifest_inventory(manifest: dict[str, Any]) -> tuple[list[str] | None, str | None, str | None]:
    source = _manifest_source(manifest)
    files = source.get("files", manifest.get("files"))
    declared_hash = source.get("content_sha256", source.get("content_hash", manifest.get("content_sha256", manifest.get("content_hash"))))
    algorithm = source.get("content_hash_algorithm", source.get("hash_algorithm", manifest.get("hash_algorithm", HASH_ALGORITHM)))
    return files if isinstance(files, list) else None, declared_hash if isinstance(declared_hash, str) else None, algorithm if isinstance(algorithm, str) else None


def check_manifest(root: Path, manifest_path: Path, source_root: Path | None = None) -> dict[str, Any]:
    """Validate a generated manifest against a safe tree and optional source.

    The return value is intentionally JSON-serialisable.  It reports separate
    inventory causes so callers can preserve the fixed precedence instead of
    reducing every failure to a generic hash mismatch.
    """
    result: dict[str, Any] = {
        "status": "manifest-invalid",
        "manifest": str(manifest_path),
        "root": str(root),
        "details": {},
    }
    try:
        root_absolute = Path(root).absolute()
        manifest_absolute = Path(manifest_path).absolute()
        manifest_relative = manifest_absolute.relative_to(root_absolute).as_posix()
        manifest_fd = open_relative(root_absolute, manifest_relative)
        try:
            raw = _read_fd(manifest_fd)
        finally:
            os.close(manifest_fd)
        value = json.loads(raw.decode("utf-8"))
    except (OSError, ProvenanceError) as exc:
        result["status"] = "symlink-escape" if isinstance(exc, UnsafePath) and "symlink" in str(exc) else ("unsafe-path" if isinstance(exc, UnsafePath) else "manifest-invalid")
        result["details"] = {"error": str(exc)}
        return result
    if not isinstance(value, dict):
        result["details"] = {"error": "manifest-invalid: root must be an object"}
        return result

    files, declared_hash, algorithm = _manifest_inventory(value)
    if algorithm != HASH_ALGORITHM:
        result["details"] = {"error": f"manifest-invalid: unsupported hash algorithm {algorithm!r}"}
        return result
    try:
        manifest_relative = Path(manifest_path).absolute().relative_to(Path(root).absolute()).as_posix()
        inventory, actual_hash = inventory_tree(Path(root), {manifest_relative})
    except SafePathUnsupported as exc:
        result["status"] = "safe-path-unsupported"
        result["details"] = {"error": str(exc)}
        return result
    except UnsafePath as exc:
        result["status"] = "symlink-escape" if "symlink" in str(exc) else "unsafe-path"
        result["details"] = {"error": str(exc)}
        return result
    except OSError as exc:
        result["status"] = "source-unavailable"
        result["details"] = {"error": str(exc)}
        return result

    details: dict[str, Any] = {
        "actual_files": inventory,
        "actual_file_count": len(inventory),
        "actual_hash": actual_hash,
        "declared_file_count": value.get("file_count", _manifest_source(value).get("file_count")),
        "declared_files": files,
        "declared_hash": declared_hash,
    }
    if files is None:
        result["details"] = {**details, "error": "manifest-invalid: files inventory is required"}
        return result
    if len(files) != len(set(files)):
        result["status"] = "inventory-duplicate"
        result["details"] = details
        return result
    try:
        canonical = [safe_relative_path(item) for item in files]
    except UnsafePath as exc:
        result["status"] = "unsafe-path"
        result["details"] = {**details, "error": str(exc)}
        return result
    if canonical != files:
        result["status"] = "unsafe-path"
        result["details"] = {**details, "error": "manifest inventory contains a non-canonical path"}
        return result
    if files != sorted(files):
        result["status"] = "inventory-order"
        result["details"] = details
        return result
    missing = sorted(set(files) - set(inventory))
    extra = sorted(set(inventory) - set(files))
    if missing:
        result["status"] = "inventory-missing"
        details["inventory_missing"] = missing
    elif extra:
        result["status"] = "inventory-extra"
        details["inventory_extra"] = extra
    elif value.get("file_count", _manifest_source(value).get("file_count")) not in {None, len(files)}:
        result["status"] = "inventory-missing"
        details["file_count_mismatch"] = True
    elif declared_hash != actual_hash:
        result["status"] = "content-hash-mismatch"
    else:
        result["status"] = "source-current"
    if source_root is not None:
        result["source_root"] = str(source_root)
    result["details"] = details
    return result


def _write_json(path: Path, value: dict[str, Any]) -> None:
    payload = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")
    fd = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    try:
        os.write(fd, payload)
        os.fsync(fd)
    finally:
        os.close(fd)


def _same_digest(path: Path, expected: str | None) -> bool:
    if expected is None:
        return True
    if path.is_dir():
        _, actual = inventory_tree(path, set())
    else:
        digest = hashlib.sha256()
        fd = open_relative(path.parent, path.name)
        try:
            while True:
                chunk = os.read(fd, 1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        finally:
            os.close(fd)
        actual = digest.hexdigest()
    return actual == expected


def recover_journal(journal_path: Path, action: Literal["complete", "restore"]) -> dict[str, Any]:
    """Recover a directory-promotion journal without silently discarding work.

    Suite git-operation journals are also accepted: their recovery is carried
    out by ``tools/suite.py`` because Git refs and index state are repository
    semantics.  This function handles the independently versioned tool's
    directory contract and rejects conflicts before any destructive operation.
    """
    journal_path = Path(journal_path)
    try:
        value = json.loads(journal_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ProvenanceError(f"cannot read journal: {journal_path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ProvenanceError("journal must contain an object")
    if value.get("state") == "committed":
        if action == "restore":
            raise ProvenanceError("recovery conflict: committed journal cannot be restored")
        return {"status": "committed", "journal": str(journal_path), "state": "committed"}
    if value.get("kind") not in {"directory-promotion", "evidence-directory"}:
        return {"status": "delegated", "journal": str(journal_path), "state": value.get("state")}
    destination = Path(value["destination"])
    staging = Path(value["staging"])
    backup = Path(value["backup"])
    expected = value.get("expected_digest")
    state = value.get("state")
    if action == "complete":
        if state not in {"prepared", "old-moved", "new-promoted"}:
            raise ProvenanceError(f"unsupported recovery state: {state}")
        if staging.exists() and not _same_digest(staging, expected):
            raise ProvenanceError("recovery conflict: staging digest does not match the journal")
        if destination.exists() and expected is not None and _same_digest(destination, expected):
            value["state"] = "committed"
        elif staging.exists():
            if destination.exists():
                raise ProvenanceError("recovery conflict: destination already exists with a different digest")
            os.replace(staging, destination)
            value["state"] = "committed"
        else:
            raise ProvenanceError("recovery conflict: expected staging or committed destination is missing")
    else:
        if state == "committed":
            raise ProvenanceError("recovery conflict: committed journal cannot be restored")
        if not backup.exists():
            raise ProvenanceError("recovery conflict: backup is missing")
        if destination.exists():
            if expected is not None and _same_digest(destination, expected):
                raise ProvenanceError("recovery conflict: destination contains the promoted tree")
            raise ProvenanceError("recovery conflict: destination changed after interruption")
        os.replace(backup, destination)
        value["state"] = "restored"
    _write_json(journal_path, value)
    try:
        fd = os.open(str(journal_path.parent), os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except (OSError, SafePathUnsupported):
        pass
    return {"status": value["state"], "journal": str(journal_path), "state": value["state"]}


__all__ = [
    "HASH_ALGORITHM",
    "ProvenanceError",
    "SafePathUnsupported",
    "UnsafePath",
    "check_manifest",
    "ensure_descriptor_safety",
    "inventory_tree",
    "open_relative",
    "recover_journal",
    "safe_relative_path",
]
