#!/usr/bin/env python3
"""Smoke tests for gpi_bridge_host."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def _request(host_path: Path, bridge_repo: Path, method: str, params: dict[str, object]) -> dict[str, object]:
    request_payload = {
        "id": "smoke-1",
        "method": method,
        "params": params,
    }
    process = subprocess.run(
        [str(host_path), "--bridge-repo", str(bridge_repo)],
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
    parser = argparse.ArgumentParser(description="Smoke test gpi_bridge_host.")
    parser.add_argument("--bridge-repo", required=True)
    parser.add_argument("--host", default="")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    host_path = Path(args.host).resolve() if args.host else (repo_root / "build" / "bin" / "gpi_bridge_host.exe")
    bridge_repo = Path(args.bridge_repo).resolve()

    if not host_path.exists():
        print(f"host binary missing: {host_path}", file=sys.stderr)
        return 2
    if not bridge_repo.exists():
        print(f"bridge repo missing: {bridge_repo}", file=sys.stderr)
        return 2

    ping = _request(host_path, bridge_repo, "ping", {})
    if not bool(ping.get("ok")):
        print(f"ping failed: {ping}", file=sys.stderr)
        return 2

    status = _request(host_path, bridge_repo, "status.get", {"appId": "bridge"})
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
