#!/usr/bin/env python3
"""Smoke tests for gpi_host over ZeroMQ IPC."""

from __future__ import annotations

import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from repo_paths import resolve_repo_root


def _pick_endpoint() -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = int(sock.getsockname()[1])
    sock.close()
    return f"tcp://127.0.0.1:{port}"


def _request_raw(
    client_path: Path,
    endpoint: str,
    method: str,
    params: dict[str, object],
    *,
    override_payload: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload = override_payload or {"id": "smoke-1", "method": method, "params": params}
    completed = subprocess.run(
        [str(client_path), "--ipc-endpoint", endpoint, "--request-json", json.dumps(payload, separators=(",", ":"))],
        text=True,
        capture_output=True,
        timeout=30.0,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"ipc request failed rc={completed.returncode}: {completed.stderr.strip()}")

    stdout = (completed.stdout or "").strip()
    if not stdout:
        raise RuntimeError("ipc client returned empty response")
    return json.loads(stdout)


def _request(
    client_path: Path,
    endpoint: str,
    method: str,
    params: dict[str, object],
) -> dict[str, Any]:
    payload = _request_raw(client_path, endpoint, method, params)
    if not bool(payload.get("ok", False)):
        raise RuntimeError(f"request failed: {payload}")
    response = payload.get("response")
    if not isinstance(response, dict):
        raise RuntimeError(f"response missing object payload: {payload}")
    return response


def _wait_until_ready(client_path: Path, endpoint: str, timeout_seconds: float = 10.0) -> None:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        try:
            _request(client_path, endpoint, "ping", {})
            return
        except Exception:
            time.sleep(0.15)
    raise RuntimeError("host did not become ready")


def _snapshot_path(repo_path: Path, app_id: str) -> Path:
    return repo_path / "runtime" / "logs" / "process-interface" / "status-source" / f"{app_id}.json"


def _assert_snapshot_written(repo_path: Path, app_id: str) -> None:
    snapshot_path = _snapshot_path(repo_path, app_id)
    if not snapshot_path.exists():
        raise RuntimeError(f"missing status snapshot: {snapshot_path}")

    payload = json.loads(snapshot_path.read_text(encoding="utf-8"))
    if str(payload.get("appId") or "") != app_id:
        raise RuntimeError(f"snapshot appId mismatch for {app_id}: {snapshot_path}")
    nested = payload.get("payload")
    if not isinstance(nested, dict):
        raise RuntimeError(f"snapshot payload missing object body: {snapshot_path}")


def _assert_parity_fixture_contains_app(repo_root: Path, app_id: str) -> None:
    parity_path = repo_root / "test" / "status_parity" / "status_payload_parity.json"
    if not parity_path.exists():
        raise RuntimeError(f"missing parity fixture: {parity_path}")
    payload = json.loads(parity_path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or not isinstance(payload.get(app_id), dict):
        raise RuntimeError(f"parity fixture missing app section: {app_id}")


def _load_allowed_apps(profile_path: Path) -> list[str]:
    root = json.loads(profile_path.read_text(encoding="utf-8"))
    allowed = root.get("allowedApps")
    if not isinstance(allowed, list):
        raise RuntimeError(f"profile missing allowedApps: {profile_path}")
    values = [str(item) for item in allowed if str(item)]
    if not values:
        raise RuntimeError(f"profile has empty allowedApps: {profile_path}")
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke tests for gpi_host over ZeroMQ.")
    parser.add_argument("--repo", required=True)
    parser.add_argument("--host-config", required=True)
    args = parser.parse_args()

    repo_root = resolve_repo_root(Path(__file__))
    bin_dir = repo_root / "artifacts" / "build" / "bin"
    host_name = "gpi_host.exe" if sys.platform.startswith("win") else "gpi_host"
    client_name = "gpi_client.exe" if sys.platform.startswith("win") else "gpi_client"
    host_path = (bin_dir / host_name).resolve()
    client_path = (bin_dir / client_name).resolve()

    if not host_path.exists():
        print(f"missing host binary: {host_path}", file=sys.stderr)
        return 2
    if not client_path.exists():
        print(f"missing client binary: {client_path}", file=sys.stderr)
        return 2

    target_repo = Path(args.repo).resolve()
    host_profile = Path(args.host_config).resolve()
    allowed_apps = _load_allowed_apps(host_profile)
    expected_app_id = allowed_apps[0]

    endpoint = _pick_endpoint()
    host_process = subprocess.Popen(
        [
            str(host_path),
            "--repo",
            str(target_repo),
            "--host-config",
            str(host_profile),
            "--ipc-endpoint",
            endpoint,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    try:
        _wait_until_ready(client_path, endpoint)

        ping = _request(client_path, endpoint, "ping", {})
        if not bool(ping.get("pong", False)):
            raise RuntimeError(f"unexpected ping response: {ping}")

        status = _request(client_path, endpoint, "status.get", {"appId": expected_app_id})
        if str(status.get("appId") or "") != expected_app_id:
            raise RuntimeError(f"status appId mismatch: {status}")

        config_get = _request(client_path, endpoint, "config.get", {"appId": expected_app_id})
        if not isinstance(config_get, dict):
            raise RuntimeError(f"config.get response malformed: {config_get}")

        config_set = _request(
            client_path,
            endpoint,
            "config.set",
            {"appId": expected_app_id, "key": "profile", "value": "sim"},
        )
        if not isinstance(config_set, dict):
            raise RuntimeError(f"config.set response malformed: {config_set}")

        action_list = _request(client_path, endpoint, "action.list", {"appId": expected_app_id})
        actions = action_list.get("actions")
        if not isinstance(actions, list):
            raise RuntimeError(f"action.list response malformed: {action_list}")

        invoke_action_name = "run_echo"
        if actions:
            first = actions[0]
            if isinstance(first, dict) and isinstance(first.get("name"), str) and first["name"]:
                invoke_action_name = first["name"]

        invoke = _request(
            client_path,
            endpoint,
            "action.invoke",
            {"appId": expected_app_id, "actionName": invoke_action_name, "args": {}},
        )
        job_id = str(invoke.get("jobId") or "")
        if not job_id:
            raise RuntimeError(f"action.invoke missing jobId: {invoke}")

        job = _request(
            client_path,
            endpoint,
            "action.job.get",
            {"appId": expected_app_id, "jobId": job_id},
        )
        if str(job.get("jobId") or "") != job_id:
            raise RuntimeError(f"action.job.get jobId mismatch: {job}")

        missing_app = _request_raw(client_path, endpoint, "config.get", {})
        if bool(missing_app.get("ok", False)):
            raise RuntimeError(f"expected missing app error: {missing_app}")
        missing_app_error = missing_app.get("error") or {}
        if missing_app_error.get("code") != "E_BAD_ARG":
            raise RuntimeError(f"missing app error code mismatch: {missing_app}")

        unknown_method = _request_raw(client_path, endpoint, "unknown.method", {"appId": expected_app_id})
        if bool(unknown_method.get("ok", False)):
            raise RuntimeError(f"expected unknown method error: {unknown_method}")
        unknown_method_error = unknown_method.get("error") or {}
        if unknown_method_error.get("code") != "E_UNSUPPORTED_METHOD":
            raise RuntimeError(f"unknown method error code mismatch: {unknown_method}")

        bad_invoke = _request_raw(
            client_path,
            endpoint,
            "action.invoke",
            {"appId": expected_app_id, "args": {}},
        )
        if bool(bad_invoke.get("ok", False)):
            raise RuntimeError(f"expected bad action.invoke error: {bad_invoke}")
        bad_invoke_error = bad_invoke.get("error") or {}
        if bad_invoke_error.get("code") != "E_BAD_ARG":
            raise RuntimeError(f"action.invoke bad arg code mismatch: {bad_invoke}")

        missing_job = _request_raw(
            client_path,
            endpoint,
            "action.job.get",
            {"appId": expected_app_id, "jobId": "job-does-not-exist"},
        )
        if bool(missing_job.get("ok", False)):
            raise RuntimeError(f"expected missing job error: {missing_job}")
        missing_job_error = missing_job.get("error") or {}
        if missing_job_error.get("code") != "E_NOT_FOUND":
            raise RuntimeError(f"missing job error code mismatch: {missing_job}")

        _assert_snapshot_written(target_repo, expected_app_id)
        _assert_parity_fixture_contains_app(repo_root, expected_app_id)

        print("test ok")
        return 0
    finally:
        if host_process.poll() is None:
            host_process.terminate()
            try:
                host_process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                host_process.kill()


if __name__ == "__main__":
    raise SystemExit(main())
