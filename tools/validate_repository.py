#!/usr/bin/env python3
"""Validate RiscRTE-Utilities bookkeeping against checked-in app files."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = ROOT / "utilities-manifest.json"
README_PATH = ROOT / "README.md"


def main() -> int:
    errors: list[str] = []
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    readme = README_PATH.read_text(encoding="utf-8")

    if registry.get("schema") != 1:
        errors.append("utilities-manifest.json schema must be 1")

    apps = registry.get("apps")
    if not isinstance(apps, list) or not apps:
        errors.append("utilities-manifest.json must contain a non-empty apps array")
        apps = []

    seen: set[str] = set()
    for entry in apps:
        app_id = entry.get("id")
        if not isinstance(app_id, str) or not app_id:
            errors.append("every app entry must have a non-empty string id")
            continue
        if app_id in seen:
            errors.append(f"duplicate app id: {app_id}")
        seen.add(app_id)

        if entry.get("classification") != "utility":
            errors.append(f"{app_id}: classification must be 'utility'")

        source = ROOT / entry.get("source_path", f"Apps/{app_id}.c")
        manifest = ROOT / entry.get("manifest_path", f"Apps/{app_id}.json")
        doc = ROOT / f"docs/apps/{app_id}.md"
        status = str(entry.get("migration_status", "complete"))

        if "source-pending" not in status and not source.is_file():
            errors.append(f"{app_id}: missing source file {source.relative_to(ROOT)}")
        if not manifest.is_file():
            errors.append(f"{app_id}: missing app manifest {manifest.relative_to(ROOT)}")
            continue
        if not doc.is_file():
            errors.append(f"{app_id}: missing documentation {doc.relative_to(ROOT)}")

        link = f"docs/apps/{app_id}.md"
        if link not in readme:
            errors.append(f"{app_id}: README does not link {link}")

        app_manifest = json.loads(manifest.read_text(encoding="utf-8"))
        for field in ("version", "min_firmware_version"):
            expected = entry.get(field)
            actual = app_manifest.get(field)
            if expected is not None and actual != expected:
                errors.append(
                    f"{app_id}: {field} registry={expected!r} manifest={actual!r}"
                )

        expected_file = entry.get("file_name")
        if expected_file is not None and app_manifest.get("file_name") != expected_file:
            errors.append(
                f"{app_id}: file_name registry={expected_file!r} "
                f"manifest={app_manifest.get('file_name')!r}"
            )

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"Validation failed with {len(errors)} error(s).", file=sys.stderr)
        return 1

    print(f"Validated {len(apps)} utility app entries.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
