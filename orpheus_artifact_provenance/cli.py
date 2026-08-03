from __future__ import annotations

import argparse
import json
from pathlib import Path

from . import check_manifest, recover_journal


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="orpheus-artifact-provenance")
    subparsers = parser.add_subparsers(dest="command", required=True)

    check = subparsers.add_parser("check")
    check.add_argument("--root", type=Path, required=True)
    check.add_argument("--manifest", type=Path, required=True)
    check.add_argument("--source-root", type=Path)
    check.add_argument("--json", action="store_true")

    recover = subparsers.add_parser("recover")
    recover.add_argument("--journal", type=Path, required=True)
    recover.add_argument("--action", choices=["complete", "restore"], required=True)
    recover.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "check":
            result = check_manifest(args.root, args.manifest, args.source_root)
        else:
            result = recover_journal(args.journal, args.action)
    except Exception as exc:  # command boundary must remain machine-readable
        result = {"status": "blocked", "error": str(exc)}
        if args.json:
            print(json.dumps(result, indent=2, sort_keys=True))
        else:
            parser.exit(1, f"orpheus-artifact-provenance: {exc}\n")
        return 1
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(result.get("status", "unknown"))
    return 0 if result.get("status") in {"source-current", "committed", "restored", "new-promoted"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
