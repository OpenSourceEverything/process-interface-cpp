#!/usr/bin/env python3
"""Shared repo-path helpers for scripts under ops/scripts."""

from __future__ import annotations

from pathlib import Path


def resolve_repo_root(anchor: Path | None = None) -> Path:
    """Resolve the repository root from any script depth."""
    current = (anchor or Path(__file__)).resolve()
    if current.is_file():
        current = current.parent

    for candidate in (current, *current.parents):
        if (candidate / ".git").exists():
            return candidate
        if (candidate / "CMakeLists.txt").exists() and (candidate / "src").exists():
            return candidate

    raise RuntimeError(f"unable to resolve repo root from: {current}")
