#!/usr/bin/env python3
"""Build process-interface-cpp bridge host from source."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _find_default_msvc_root() -> Path | None:
    candidate = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\SDK\ScopeCppSDK\vc15\VC")
    if candidate.exists():
        return candidate
    return None


def _find_default_sdk_root() -> Path | None:
    candidate = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\SDK\ScopeCppSDK\vc15\SDK")
    if candidate.exists():
        return candidate
    return None


def _resolve_cl_path(msvc_root: Path | None) -> Path | None:
    env_override = os.getenv("GPI_MSVC_CL", "").strip()
    if env_override:
        path = Path(env_override)
        if path.exists():
            return path

    if msvc_root is not None:
        candidate = msvc_root / "bin" / "cl.exe"
        if candidate.exists():
            return candidate

    from_path = shutil.which("cl")
    if from_path:
        return Path(from_path)

    return None


def _run_command(command: list[str], *, cwd: Path, extra_path: Path | None) -> int:
    env = os.environ.copy()
    if extra_path is not None:
        env["PATH"] = str(extra_path) + os.pathsep + env.get("PATH", "")
    process = subprocess.run(command, cwd=cwd, env=env, text=True)
    return int(process.returncode)


def _compile_target(
    *,
    cl_path: Path,
    repo_root: Path,
    obj_dir: Path,
    include_args: list[str],
    lib_dir: Path,
    sdk_lib_dir: Path,
    output_path: Path,
    source_files: list[str],
) -> int:
    command = [
        str(cl_path),
        "/nologo",
        "/EHsc",
        "/std:c++17",
        "/W3",
        *include_args,
        "/Fo" + str(obj_dir) + "\\",
        *source_files,
        "/link",
        f"/LIBPATH:{lib_dir}",
        f"/LIBPATH:{sdk_lib_dir}",
        f"/OUT:{output_path}",
    ]
    print("BUILD:", " ".join(command), flush=True)
    rc = _run_command(command, cwd=repo_root, extra_path=cl_path.parent)
    if rc != 0:
        print(f"build failed: {output_path.name}", file=sys.stderr)
    return rc


def main() -> int:
    parser = argparse.ArgumentParser(description="Build gpi_bridge_host and gpi_fixture_host.")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    build_dir = repo_root / "build"
    obj_dir = build_dir / "obj"
    bin_dir = build_dir / "bin"
    bridge_output_path = bin_dir / "gpi_bridge_host.exe"
    fixture_output_path = bin_dir / "gpi_fixture_host.exe"

    if args.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    obj_dir.mkdir(parents=True, exist_ok=True)
    bin_dir.mkdir(parents=True, exist_ok=True)

    msvc_root = _find_default_msvc_root()
    sdk_root = _find_default_sdk_root()
    cl_path = _resolve_cl_path(msvc_root)
    if cl_path is None:
        print("build failed: cl.exe not found. Install MSVC toolchain or set GPI_MSVC_CL.", file=sys.stderr)
        return 2

    include_dir = msvc_root / "include" if msvc_root is not None else None
    lib_dir = msvc_root / "lib" if msvc_root is not None else None
    sdk_include_root = sdk_root / "include" if sdk_root is not None else None
    sdk_lib_dir = sdk_root / "lib" if sdk_root is not None else None
    if include_dir is None or not include_dir.exists():
        print("build failed: include dir not found for MSVC toolchain.", file=sys.stderr)
        return 2
    if lib_dir is None or not lib_dir.exists():
        print("build failed: lib dir not found for MSVC toolchain.", file=sys.stderr)
        return 2
    if sdk_include_root is None or not sdk_include_root.exists():
        print("build failed: SDK include dir not found for MSVC toolchain.", file=sys.stderr)
        return 2
    if sdk_lib_dir is None or not sdk_lib_dir.exists():
        print("build failed: SDK lib dir not found for MSVC toolchain.", file=sys.stderr)
        return 2

    include_dirs = [
        include_dir,
        sdk_include_root / "ucrt",
        sdk_include_root / "shared",
        sdk_include_root / "um",
    ]
    include_args: list[str] = []
    include_path: Path
    for include_path in include_dirs:
        if not include_path.exists():
            print(f"build failed: include path missing: {include_path}", file=sys.stderr)
            return 2
        include_args.append(f"/I{include_path}")

    bridge_sources = [
        str((repo_root / "bridge_host" / "main.cpp").resolve()),
        str((repo_root / "bridge_adapter" / "bridge_adapter.cpp").resolve()),
        str((repo_root / "wire_v0" / "wire_v0.cpp").resolve()),
    ]

    fixture_sources = [
        str((repo_root / "fixture_host" / "main.cpp").resolve()),
        str((repo_root / "fixture_adapter" / "fixture_adapter.cpp").resolve()),
        str((repo_root / "wire_v0" / "wire_v0.cpp").resolve()),
    ]

    rc = _compile_target(
        cl_path=cl_path,
        repo_root=repo_root,
        obj_dir=obj_dir,
        include_args=include_args,
        lib_dir=lib_dir,
        sdk_lib_dir=sdk_lib_dir,
        output_path=bridge_output_path,
        source_files=bridge_sources,
    )
    if rc != 0:
        return rc

    rc = _compile_target(
        cl_path=cl_path,
        repo_root=repo_root,
        obj_dir=obj_dir,
        include_args=include_args,
        lib_dir=lib_dir,
        sdk_lib_dir=sdk_lib_dir,
        output_path=fixture_output_path,
        source_files=fixture_sources,
    )
    if rc != 0:
        return rc

    print(f"build ok: {bridge_output_path}")
    print(f"build ok: {fixture_output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
