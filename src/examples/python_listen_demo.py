#!/usr/bin/env python3
"""Run the talk server/client demo and listen to events in Python."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _platform_binary_name(base_name: str) -> str:
    if os.name == "nt":
        return f"{base_name}.exe"
    return base_name


def _find_binary(explicit_path: str | None, base_name: str) -> Path | None:
    if explicit_path:
        candidate = Path(explicit_path).expanduser().resolve()
        if candidate.exists():
            return candidate
        return None

    repo_root = _repo_root()
    binary_name = _platform_binary_name(base_name)
    search_roots = [
        repo_root / "artifacts" / "build" / "bin",
        repo_root / "artifacts" / "build",
    ]

    for search_root in search_roots:
        if not search_root.exists():
            continue
        matches = list(search_root.rglob(binary_name))
        if matches:
            matches.sort(key=lambda value: len(str(value)))
            return matches[0]
    return None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Spawn the talk server/client demo and listen to server events in Python."
    )
    parser.add_argument("--server-bin", default="", help="path to gpi_example_talk_server binary")
    parser.add_argument("--client-bin", default="", help="path to gpi_example_talk_client binary")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5580")
    parser.add_argument("--count", type=int, default=4)
    parser.add_argument("--delay-ms", type=int, default=150)
    parser.add_argument("--startup-wait-ms", type=int, default=300)
    parser.add_argument("--server-timeout-s", type=float, default=10.0)
    args = parser.parse_args()

    if args.count <= 0:
        print("count must be > 0", file=sys.stderr)
        return 2
    if args.delay_ms <= 0:
        print("delay-ms must be > 0", file=sys.stderr)
        return 2
    if args.startup_wait_ms < 0:
        print("startup-wait-ms must be >= 0", file=sys.stderr)
        return 2
    if args.server_timeout_s <= 0:
        print("server-timeout-s must be > 0", file=sys.stderr)
        return 2

    server_binary = _find_binary(args.server_bin.strip() or None, "gpi_example_talk_server")
    if server_binary is None:
        print("could not find gpi_example_talk_server binary", file=sys.stderr)
        return 2

    client_binary = _find_binary(args.client_bin.strip() or None, "gpi_example_talk_client")
    if client_binary is None:
        print("could not find gpi_example_talk_client binary", file=sys.stderr)
        return 2

    child_env = os.environ.copy()
    path_entries: list[str] = []
    for path_value in [
        str(server_binary.parent),
        str(client_binary.parent),
        str(_repo_root() / "artifacts" / "build" / "bin"),
    ]:
        if path_value not in path_entries:
            path_entries.append(path_value)

    existing_path = child_env.get("PATH", "")
    child_env["PATH"] = os.pathsep.join(path_entries + [existing_path]) if existing_path else os.pathsep.join(path_entries)

    server_command = [
        str(server_binary),
        "--endpoint",
        args.endpoint,
        "--max-requests",
        str(args.count),
    ]

    print("python-wrapper launching server:", " ".join(server_command), flush=True)
    server_process = subprocess.Popen(
        server_command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=child_env,
    )

    events: list[dict[str, object]] = []

    def _read_server_output() -> None:
        assert server_process.stdout is not None
        for raw_line in server_process.stdout:
            line = raw_line.rstrip()
            if not line:
                continue

            if line.startswith("EVENT "):
                payload_text = line[len("EVENT ") :]
                try:
                    event = json.loads(payload_text)
                    events.append(event)
                    print("python-listener event:", json.dumps(event, sort_keys=True), flush=True)
                except json.JSONDecodeError:
                    print("python-listener malformed event:", payload_text, flush=True)
                continue

            print("server:", line, flush=True)

    output_thread = threading.Thread(target=_read_server_output, daemon=True)
    output_thread.start()

    time.sleep(args.startup_wait_ms / 1000.0)

    client_command = [
        str(client_binary),
        "--endpoint",
        args.endpoint,
        "--count",
        str(args.count),
        "--delay-ms",
        str(args.delay_ms),
    ]
    print("python-wrapper launching client:", " ".join(client_command), flush=True)
    client_completed = subprocess.run(
        client_command,
        capture_output=True,
        text=True,
        env=child_env,
    )

    if client_completed.stdout:
        print(client_completed.stdout, end="", flush=True)
    if client_completed.stderr:
        print(client_completed.stderr, end="", file=sys.stderr, flush=True)

    server_timed_out = False
    try:
        server_return_code = int(server_process.wait(timeout=args.server_timeout_s))
    except subprocess.TimeoutExpired:
        server_timed_out = True
        server_process.terminate()
        try:
            server_return_code = int(server_process.wait(timeout=2.0))
        except subprocess.TimeoutExpired:
            server_process.kill()
            server_return_code = int(server_process.wait(timeout=2.0))

    output_thread.join(timeout=1.0)

    print(f"python-listener captured {len(events)} event(s)", flush=True)

    if client_completed.returncode != 0:
        return int(client_completed.returncode)
    if server_timed_out:
        print("server process timed out", file=sys.stderr)
        return 3
    if server_return_code != 0:
        return server_return_code
    if len(events) != args.count:
        print(
            f"expected {args.count} event(s) but captured {len(events)}",
            file=sys.stderr,
        )
        return 4

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
