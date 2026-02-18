#!/usr/bin/env python3
"""Guardrails for generic ProcessInterface::Status runtime path."""

from __future__ import annotations

from pathlib import Path


FORBIDDEN_FILES = (
    "status/bridge_status_collector.cpp",
    "status/fixture_status_collector.cpp",
)

REQUIRED_RUNTIME_TOKENS = (
    ("process_interface/host/dispatcher.cpp", "CollectAndPublishStatus"),
    ("bridge_host/main.cpp", "HandleRequest"),
    ("fixture_host/main.cpp", "HandleRequest"),
)

FORBIDDEN_HOST_TOKENS = (
    ("bridge_host/main.cpp", "ReadStatusSnapshotPayload"),
    ("fixture_host/main.cpp", "ReadStatusSnapshotPayload"),
    ("bridge_host/main.cpp", "status_snapshot_reader"),
    ("fixture_host/main.cpp", "status_snapshot_reader"),
)

FORBIDDEN_ROUTER_FILES = (
    "provider_api/control_router.h",
    "provider_api/control_router.cpp",
)

FORBIDDEN_LEGACY_PROVIDER_SURFACES = (
    "provider_api/control_script_runner.h",
    "provider_api/control_script_runner.cpp",
    "provider_api/provider_api.h",
    "bridge_adapter/bridge_adapter.h",
    "bridge_adapter/bridge_adapter.cpp",
    "fixture_adapter/fixture_adapter.h",
    "fixture_adapter/fixture_adapter.cpp",
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

    for rel, token in REQUIRED_RUNTIME_TOKENS:
        path = repo_root / rel
        if not path.exists():
            errors.append(f"missing required file: {path}")
            continue
        text = _read(path)
        if token not in text:
            errors.append(f"{path}: missing required token '{token}'")

    for rel in FORBIDDEN_ROUTER_FILES:
        path = repo_root / rel
        if path.exists():
            errors.append(f"forbidden legacy router file exists: {path}")

    for rel in FORBIDDEN_LEGACY_PROVIDER_SURFACES:
        path = repo_root / rel
        if path.exists():
            errors.append(f"forbidden legacy provider surface exists: {path}")

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
