#!/usr/bin/env python3
"""Smoke tests for gpi_bridge_host and gpi_fixture_host."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def _request(host_path: Path, repo_arg_name: str, repo_path: Path, method: str, params: dict[str, object]) -> dict[str, object]:
    request_payload = {
        "id": "smoke-1",
        "method": method,
        "params": params,
    }
    process = subprocess.run(
        [str(host_path), repo_arg_name, str(repo_path)],
        input=json.dumps(request_payload, separators=(",", ":")) + "\n",
        text=True,
        capture_output=True,
        timeout=10.0,
    )
    if process.returncode != 0:
        raise RuntimeError(f"host exited non-zero: {process.returncode} stderr={process.stderr.strip()}")

    line = (process.stdout or "").strip().splitlines()
    if not line:
        raise RuntimeError("host returned empty output")
    return json.loads(line[-1])


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke test native hosts.")
    parser.add_argument("--bridge-repo", default="")
    parser.add_argument("--fixture-repo", default="")
    parser.add_argument("--host", default="")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    bridge_repo_text = str(args.bridge_repo).strip()
    fixture_repo_text = str(args.fixture_repo).strip()
    if bool(bridge_repo_text) == bool(fixture_repo_text):
        print("exactly one of --bridge-repo or --fixture-repo is required", file=sys.stderr)
        return 2

    is_bridge = bool(bridge_repo_text)
    if args.host:
        host_path = Path(args.host).resolve()
    else:
        host_name = "gpi_bridge_host.exe" if is_bridge else "gpi_fixture_host.exe"
        host_path = repo_root / "build" / "bin" / host_name

    target_repo = Path(bridge_repo_text or fixture_repo_text).resolve()
    repo_arg_name = "--bridge-repo" if is_bridge else "--fixture-repo"
    expected_app_id = "bridge" if is_bridge else "40318"

    if not host_path.exists():
        print(f"host binary missing: {host_path}", file=sys.stderr)
        return 2
    if not target_repo.exists():
        print(f"target repo missing: {target_repo}", file=sys.stderr)
        return 2

    ping = _request(host_path, repo_arg_name, target_repo, "ping", {})
    if not bool(ping.get("ok")):
        print(f"ping failed: {ping}", file=sys.stderr)
        return 2

    status = _request(host_path, repo_arg_name, target_repo, "status.get", {"appId": expected_app_id})
    if not bool(status.get("ok")):
        print(f"status.get failed: {status}", file=sys.stderr)
        return 2
    response_payload = status.get("response")
    if not isinstance(response_payload, dict):
        print(f"status response not object: {status}", file=sys.stderr)
        return 2

    print("native smoke ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
