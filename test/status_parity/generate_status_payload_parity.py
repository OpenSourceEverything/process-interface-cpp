#!/usr/bin/env python3
"""Generate top-level status payload key/type parity fixture."""

from __future__ import annotations

import argparse
import json
import socket
import subprocess
import time
from pathlib import Path
from typing import Any


def _pick_endpoint() -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = int(sock.getsockname()[1])
    sock.close()
    return f"tcp://127.0.0.1:{port}"


def _request(client_path: Path, endpoint: str, app_id: str) -> dict[str, Any]:
    request = {"id": "parity", "method": "status.get", "params": {"appId": app_id}}
    completed = subprocess.run(
        [str(client_path), "--ipc-endpoint", endpoint, "--request-json", json.dumps(request, separators=(",", ":"))],
        text=True,
        capture_output=True,
        timeout=20.0,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"client failed rc={completed.returncode} stderr={completed.stderr.strip()}")
    payload = json.loads((completed.stdout or "").strip() or "{}")
    if not isinstance(payload, dict) or not bool(payload.get("ok", False)):
        raise RuntimeError(f"status.get failed for {app_id}: {payload}")
    response = payload.get("response")
    if not isinstance(response, dict):
        raise RuntimeError(f"status.get response is not object for {app_id}")
    return response


def _wait_ready(client_path: Path, endpoint: str) -> None:
    deadline = time.time() + 8.0
    while time.time() < deadline:
        try:
            request = {"id": "ready", "method": "ping", "params": {}}
            completed = subprocess.run(
                [str(client_path), "--ipc-endpoint", endpoint, "--request-json", json.dumps(request, separators=(",", ":"))],
                text=True,
                capture_output=True,
                timeout=5.0,
            )
            if completed.returncode == 0 and (completed.stdout or "").strip():
                return
        except Exception:
            pass
        time.sleep(0.15)
    raise RuntimeError("host did not become ready")


def _type_name(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    return type(value).__name__


def _shape(payload: dict[str, Any]) -> dict[str, str]:
    return {str(key): _type_name(value) for key, value in sorted(payload.items(), key=lambda item: str(item[0]))}


def _spawn_host(host_path: Path, repo_path: Path, profile_path: Path, endpoint: str) -> subprocess.Popen[str]:
    return subprocess.Popen(
        [
            str(host_path),
            "--repo",
            str(repo_path),
            "--host-config",
            str(profile_path),
            "--ipc-endpoint",
            endpoint,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--client", required=True)
    parser.add_argument("--bridge-repo", required=True)
    parser.add_argument("--bridge-config", required=True)
    parser.add_argument("--fixture-repo", required=True)
    parser.add_argument("--fixture-config", required=True)
    parser.add_argument("--out", default="")
    args = parser.parse_args()

    host_path = Path(args.host).resolve()
    client_path = Path(args.client).resolve()
    bridge_repo = Path(args.bridge_repo).resolve()
    bridge_config = Path(args.bridge_config).resolve()
    fixture_repo = Path(args.fixture_repo).resolve()
    fixture_config = Path(args.fixture_config).resolve()
    out_path = Path(args.out).resolve() if str(args.out).strip() else (Path(__file__).resolve().parent / "status_payload_parity.json")

    bridge_endpoint = _pick_endpoint()
    fixture_endpoint = _pick_endpoint()
    bridge_host = _spawn_host(host_path, bridge_repo, bridge_config, bridge_endpoint)
    fixture_host = _spawn_host(host_path, fixture_repo, fixture_config, fixture_endpoint)
    try:
        _wait_ready(client_path, bridge_endpoint)
        _wait_ready(client_path, fixture_endpoint)

        data = {
            "bridge": _shape(_request(client_path, bridge_endpoint, "bridge")),
            "40318": _shape(_request(client_path, fixture_endpoint, "40318")),
            "plc-simulator": _shape(_request(client_path, fixture_endpoint, "plc-simulator")),
            "ble-simulator": _shape(_request(client_path, fixture_endpoint, "ble-simulator")),
        }
    finally:
        for proc in (bridge_host, fixture_host):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    proc.kill()

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(str(out_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
