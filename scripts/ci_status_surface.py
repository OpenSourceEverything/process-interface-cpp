#!/usr/bin/env python3
"""Guardrails for generic ProcessInterface::Status runtime path."""

from __future__ import annotations

from pathlib import Path


FORBIDDEN_FILES = (
    "status/bridge_status_collector.cpp",
    "status/fixture_status_collector.cpp",
)

REQUIRED_HOST_TOKENS = (
    ("bridge_host/main.cpp", "CollectAndPublishStatus"),
    ("fixture_host/main.cpp", "CollectAndPublishStatus"),
)

FORBIDDEN_HOST_TOKENS = (
    ("bridge_host/main.cpp", "ReadStatusSnapshotPayload"),
    ("fixture_host/main.cpp", "ReadStatusSnapshotPayload"),
    ("bridge_host/main.cpp", "status_snapshot_reader"),
    ("fixture_host/main.cpp", "status_snapshot_reader"),
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    errors: list[str] = []

    status_dir = repo_root / "status"
    if not status_dir.exists():
        errors.append(f"missing status module dir: {status_dir}")

    for rel in FORBIDDEN_FILES:
        path = repo_root / rel
        if path.exists():
            errors.append(f"forbidden app-specific collector file exists: {path}")

    for rel, token in REQUIRED_HOST_TOKENS:
        path = repo_root / rel
        if not path.exists():
            errors.append(f"missing required file: {path}")
            continue
        text = _read(path)
        if token not in text:
            errors.append(f"{path}: missing required token '{token}'")

    for rel, token in FORBIDDEN_HOST_TOKENS:
        path = repo_root / rel
        if not path.exists():
            continue
        text = _read(path)
        if token in text:
            errors.append(f"{path}: forbidden token '{token}'")

    if errors:
        print("FAIL: status surface checks")
        for item in errors:
            print(f"- {item}")
        return 2

    print("PASS: status surface checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
