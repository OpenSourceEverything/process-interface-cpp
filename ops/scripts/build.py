#!/usr/bin/env python3
"""Build process-interface binaries using CMake."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from repo_paths import resolve_repo_root


def _find_existing(candidates: list[Path]) -> Path | None:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def _resolve_cmake() -> Path | None:
    env_override = os.getenv("GPI_CMAKE", "").strip()
    if env_override:
        candidate = Path(env_override)
        if candidate.exists():
            return candidate

    from_path = shutil.which("cmake")
    if from_path:
        return Path(from_path)

    return _find_existing(
        [
            Path(r"C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe"),
            Path(r"C:\.espressif\tools\cmake\3.24.0\bin\cmake.exe"),
            Path(r"C:\msys64\mingw64\bin\cmake.exe"),
            Path(r"C:\msys64\usr\bin\cmake.exe"),
        ]
    )


def _resolve_mingw_tool(name: str, env_key: str) -> Path | None:
    env_value = os.getenv(env_key, "").strip()
    if env_value:
        candidate = Path(env_value)
        if candidate.exists():
            return candidate

    from_path = shutil.which(name)
    if from_path:
        return Path(from_path)

    return _find_existing([Path(rf"C:\msys64\mingw64\bin\{name}.exe")])


def _run(command: list[str], *, cwd: Path, extra_path: list[Path] | None = None) -> int:
    print("BUILD:", " ".join(command), flush=True)
    env = os.environ.copy()
    if extra_path:
        prefix = os.pathsep.join(str(path) for path in extra_path if path)
        if prefix:
            env["PATH"] = prefix + os.pathsep + env.get("PATH", "")
    completed = subprocess.run(command, cwd=cwd, env=env, text=True)
    return int(completed.returncode)


def _find_binary(build_dir: Path, name: str) -> Path | None:
    candidates = list(build_dir.rglob(name))
    if candidates:
        candidates.sort(key=lambda p: len(str(p)))
        return candidates[0]
    return None


def _stage_binary(build_dir: Path, bin_dir: Path, binary_name: str) -> Path | None:
    source = _find_binary(build_dir, binary_name)
    if source is None or not source.exists():
        return None

    destination = bin_dir / binary_name
    if source.resolve() != destination.resolve():
        shutil.copy2(source, destination)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description="Build process-interface binaries.")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    build_config = args.config

    repo_root = resolve_repo_root(Path(__file__))
    libzmq_cmake = repo_root / "external" / "libzmq" / "CMakeLists.txt"
    if not libzmq_cmake.exists():
        print(
            "build failed: libzmq submodule missing. Run: git submodule update --init --recursive",
            file=sys.stderr,
        )
        return 2

    build_dir = repo_root / "artifacts" / "build"
    bin_dir = build_dir / "bin"

    cmake_path = _resolve_cmake()
    if cmake_path is None:
        print("build failed: cmake executable not found", file=sys.stderr)
        return 2

    ninja_path = _resolve_mingw_tool("ninja", "GPI_NINJA")
    gcc_path = _resolve_mingw_tool("gcc", "GPI_CC")
    gxx_path = _resolve_mingw_tool("g++", "GPI_CXX")
    if ninja_path is None or gcc_path is None or gxx_path is None:
        print(
            "build failed: required toolchain not found (need ninja, gcc, g++)",
            file=sys.stderr,
        )
        return 2

    extra_path = [cmake_path.parent, ninja_path.parent, gcc_path.parent, gxx_path.parent]

    if args.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    build_dir.mkdir(parents=True, exist_ok=True)
    bin_dir.mkdir(parents=True, exist_ok=True)

    configure_command = [
        str(cmake_path),
        "-S",
        str(repo_root),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja_path.as_posix()}",
        f"-DCMAKE_C_COMPILER={gcc_path.as_posix()}",
        f"-DCMAKE_CXX_COMPILER={gxx_path.as_posix()}",
        f"-DCMAKE_BUILD_TYPE={build_config}",
    ]

    rc = _run(
        configure_command,
        cwd=repo_root,
        extra_path=extra_path,
    )
    if rc != 0:
        return rc

    host_name = "gpi_host.exe" if sys.platform.startswith("win") else "gpi_host"
    client_name = "gpi_client.exe" if sys.platform.startswith("win") else "gpi_client"
    example_server_name = (
        "gpi_example_talk_server.exe" if sys.platform.startswith("win") else "gpi_example_talk_server"
    )
    example_client_name = (
        "gpi_example_talk_client.exe" if sys.platform.startswith("win") else "gpi_example_talk_client"
    )

    targets = [
        "gpi_host",
        "gpi_client",
        "gpi_example_talk_server",
        "gpi_example_talk_client",
    ]

    rc = _run(
        [
            str(cmake_path),
            "--build",
            str(build_dir),
            "--config",
            build_config,
            "--target",
            *targets,
        ],
        cwd=repo_root,
        extra_path=extra_path,
    )
    if rc != 0:
        return rc

    staged = {}
    for binary_name in [host_name, client_name, example_server_name, example_client_name]:
        binary_path = _stage_binary(build_dir, bin_dir, binary_name)
        if binary_path is None:
            print(f"build failed: missing binary {binary_name}", file=sys.stderr)
            return 2
        staged[binary_name] = binary_path

    runtime_dlls = ["libgcc_s_seh-1.dll", "libwinpthread-1.dll", "libstdc++-6.dll"]
    for dll_name in runtime_dlls:
        source_dll = gcc_path.parent / dll_name
        if source_dll.exists():
            shutil.copy2(source_dll, bin_dir / dll_name)

    for binary_name in [host_name, client_name, example_server_name, example_client_name]:
        print(f"build ok: {staged[binary_name]}")
    print(f"build config: {build_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
