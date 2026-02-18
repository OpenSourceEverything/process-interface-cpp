#!/usr/bin/env python3
"""Guardrails for generic host + profile-driven status surface."""

from __future__ import annotations

from pathlib import Path

from repo_paths import resolve_repo_root


FORBIDDEN_FILES = (
    "src/bridge_host/main.cpp",
    "src/fixture_host/main.cpp",
    "src/process_interface/action/action_service.cpp",
    "src/process_interface/action/service.h",
    "src/process_interface/config/config_service.cpp",
    "src/process_interface/config/service.h",
    "src/status/context.cpp",
    "src/platform/file_replace_windows.cpp",
    "src/platform/file_replace_posix.cpp",
    "src/platform/process_exec_windows.cpp",
    "src/platform/process_exec_posix.cpp",
    "src/platform/process_probe_windows.cpp",
    "src/platform/process_probe_posix.cpp",
    "src/platform/port_probe_windows.cpp",
    "src/platform/port_probe_posix.cpp",
)

REQUIRED_RUNTIME_TOKENS = (
    ("src/host/main.cpp", "RunHost"),
    ("src/host_runtime/host_runtime.cpp", "--repo"),
    ("src/host_runtime/host_runtime.cpp", "--host-config"),
    ("src/host_runtime/host_runtime.cpp", "CreateIpcServer"),
    ("src/process_interface/host/dispatcher.cpp", "action.job.get"),
    ("src/process_interface/host/dispatcher.cpp", "CollectAndPublishStatus"),
    ("src/status/spec_loader.cpp", "ParseStatusExpressionLine"),
)

FORBIDDEN_SRC_TOKENS = (
    "windows.h",
    "DWORD",
    "HANDLE",
    "CreateProcess",
    "_WIN32",
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    repo_root = resolve_repo_root(Path(__file__))
    errors: list[str] = []

    for rel in FORBIDDEN_FILES:
        path = repo_root / rel
        if path.exists():
            errors.append(f"forbidden file exists: {path}")

    for rel, token in REQUIRED_RUNTIME_TOKENS:
        path = repo_root / rel
        if not path.exists():
            errors.append(f"missing required file: {path}")
            continue
        text = _read(path)
        if token not in text:
            errors.append(f"{path}: missing required token '{token}'")

    src_dir = repo_root / "src"
    if src_dir.exists():
        for source_path in src_dir.rglob("*"):
            if source_path.suffix not in {".h", ".hpp", ".cpp"}:
                continue
            text = _read(source_path)
            for token in FORBIDDEN_SRC_TOKENS:
                if token in text:
                    errors.append(f"{source_path}: forbidden token '{token}'")

    if errors:
        print("FAIL: status surface checks")
        for item in errors:
            print(f"- {item}")
        return 2

    print("PASS: status surface checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
