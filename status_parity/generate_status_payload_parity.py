#!/usr/bin/env python3
"""Generate top-level status payload key/type parity fixture."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any


def _request(host_path: Path, repo_arg_name: str, repo_path: Path, app_id: str) -> dict[str, Any]:
    request = {"id": "parity", "method": "status.get", "params": {"appId": app_id}}
    completed = subprocess.run(
        [str(host_path), repo_arg_name, str(repo_path)],
        input=json.dumps(request, separators=(",", ":")) + "\n",
        text=True,
        capture_output=True,
        timeout=20.0,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"host failed rc={completed.returncode} stderr={completed.stderr.strip()}")
    lines = (completed.stdout or "").strip().splitlines()
    if not lines:
        raise RuntimeError("empty host output")
    payload = json.loads(lines[-1])
    if not isinstance(payload, dict) or not bool(payload.get("ok", False)):
        raise RuntimeError(f"status.get failed for {app_id}: {payload}")
    response = payload.get("response")
    if not isinstance(response, dict):
        raise RuntimeError(f"status.get response is not object for {app_id}")
    return response


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bridge-host", required=True)
    parser.add_argument("--bridge-repo", required=True)
    parser.add_argument("--fixture-host", required=True)
    parser.add_argument("--fixture-repo", required=True)
    parser.add_argument("--out", default="")
    args = parser.parse_args()

    bridge_host = Path(args.bridge_host).resolve()
    bridge_repo = Path(args.bridge_repo).resolve()
    fixture_host = Path(args.fixture_host).resolve()
    fixture_repo = Path(args.fixture_repo).resolve()
    out_path = Path(args.out).resolve() if str(args.out).strip() else (Path(__file__).resolve().parent / "status_payload_parity.json")

    data = {
        "bridge": _shape(_request(bridge_host, "--bridge-repo", bridge_repo, "bridge")),
        "40318": _shape(_request(fixture_host, "--fixture-repo", fixture_repo, "40318")),
        "plc-simulator": _shape(_request(fixture_host, "--fixture-repo", fixture_repo, "plc-simulator")),
        "ble-simulator": _shape(_request(fixture_host, "--fixture-repo", fixture_repo, "ble-simulator")),
    }

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(str(out_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
