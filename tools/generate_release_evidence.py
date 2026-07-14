#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate checksums, SPDX SBOM, and provenance for release artifacts."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess
import uuid


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sdk_version(root: pathlib.Path) -> str:
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\s*\(\s*orpheus\s+VERSION\s+(\d+\.\d+\.\d+)", text, re.I)
    if match is None:
        raise RuntimeError("CMake project version not found")
    return match.group(1)


def resolved_sndfile_version() -> str | None:
    try:
        result = subprocess.run(
            ["pkg-config", "--modversion", "sndfile"],
            check=True,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("artifacts", nargs="+", type=pathlib.Path)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    artifacts = sorted((path.resolve() for path in args.artifacts), key=lambda path: path.name)
    for artifact in artifacts:
        if not artifact.is_file():
            raise FileNotFoundError(artifact)

    version = sdk_version(root)
    checksums = {artifact.name: sha256(artifact) for artifact in artifacts}
    checksum_text = "".join(f"{digest}  {name}\n" for name, digest in checksums.items())
    (output / "SHA256SUMS").write_text(checksum_text, encoding="utf-8")

    created = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    namespace_seed = f"orpheus-sdk:{version}:{os.environ.get('GITHUB_SHA', 'local')}"
    namespace = f"https://orpheus-sdk.invalid/spdx/{uuid.uuid5(uuid.NAMESPACE_URL, namespace_seed)}"
    files = []
    relationships = []
    for index, artifact in enumerate(artifacts, start=1):
        spdx_id = f"SPDXRef-Artifact-{index}"
        files.append(
            {
                "SPDXID": spdx_id,
                "fileName": artifact.name,
                "checksums": [{"algorithm": "SHA256", "checksumValue": checksums[artifact.name]}],
            }
        )
        relationships.append(
            {
                "spdxElementId": "SPDXRef-Package-OrpheusSDK",
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": spdx_id,
            }
        )

    external_refs = []
    sndfile_version = resolved_sndfile_version()
    if sndfile_version is not None:
        external_refs.append(
            {
                "referenceCategory": "PACKAGE-MANAGER",
                "referenceType": "purl",
                "referenceLocator": f"pkgconfig/sndfile@{sndfile_version}",
            }
        )

    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"orpheus-sdk-{version}",
        "documentNamespace": namespace,
        "creationInfo": {"created": created, "creators": ["Tool: generate_release_evidence.py"]},
        "packages": [
            {
                "SPDXID": "SPDXRef-Package-OrpheusSDK",
                "name": "orpheus-sdk",
                "versionInfo": version,
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "externalRefs": external_refs,
            }
        ],
        "files": files,
        "relationships": relationships,
    }
    (output / "orpheus-sdk.spdx.json").write_text(
        json.dumps(sbom, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    provenance = {
        "_type": "https://in-toto.io/Statement/v1",
        "subject": [
            {"name": name, "digest": {"sha256": digest}} for name, digest in checksums.items()
        ],
        "predicateType": "https://slsa.dev/provenance/v1",
        "predicate": {
            "buildDefinition": {
                "buildType": "https://cmake.org/cpack/zip/v1",
                "externalParameters": {"sdkVersion": version},
                "internalParameters": {},
                "resolvedDependencies": [
                    {
                        "uri": f"git+https://github.com/{os.environ.get('GITHUB_REPOSITORY', 'local/orpheus-sdk')}@{os.environ.get('GITHUB_SHA', 'local')}"
                    }
                ],
            },
            "runDetails": {
                "builder": {"id": "https://github.com/actions/runner"},
                "metadata": {
                    "invocationId": os.environ.get("GITHUB_RUN_ID", "local"),
                    "startedOn": created,
                },
            },
        },
    }
    (output / "orpheus-sdk.provenance.json").write_text(
        json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
