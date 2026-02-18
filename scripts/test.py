#!/usr/bin/env python3
"""Smoke tests for gpi_bridge_host and gpi_fixture_host."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


def _request_raw(
    host_path: Path,
    repo_arg_name: str,
    repo_path: Path,
    method: str,
    params: dict[str, object],
) -> dict[str, Any]:
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
        timeout=30.0,
    )
    if process.returncode != 0:
        raise RuntimeError(f"host exited non-zero: {process.returncode} stderr={process.stderr.strip()}")

    lines = (process.stdout or "").strip().splitlines()
    if not lines:
        raise RuntimeError("host returned empty output")
    return json.loads(lines[-1])


def _request(
    host_path: Path,
    repo_arg_name: str,
    repo_path: Path,
    method: str,
    params: dict[str, object],
) -> dict[str, Any]:
    payload = _request_raw(host_path, repo_arg_name, repo_path, method, params)
    if not bool(payload.get("ok", False)):
        raise RuntimeError(f"{method} failed: {payload}")
    return payload


def _assert_error_code(payload: dict[str, Any], expected_code: str) -> None:
    if bool(payload.get("ok", False)):
        raise RuntimeError(f"expected error response, got success: {payload}")
    error_obj = payload.get("error")
    if not isinstance(error_obj, dict):
        raise RuntimeError(f"error response missing error object: {payload}")
    code = str(error_obj.get("code") or "")
    if code != expected_code:
        raise RuntimeError(f"expected error code {expected_code}, got {code}: {payload}")


def _snapshot_path(repo_path: Path, app_id: str) -> Path:
    return repo_path / "logs" / "process-interface" / "status-source" / f"{app_id}.json"


def _assert_snapshot_written(repo_path: Path, app_id: str) -> None:
    snapshot_path = _snapshot_path(repo_path, app_id)
    if not snapshot_path.exists():
        raise RuntimeError(f"snapshot not written: {snapshot_path}")
    payload = json.loads(snapshot_path.read_text(encoding="utf-8-sig"))
    if not isinstance(payload, dict):
        raise RuntimeError("snapshot envelope must be object")
    if str(payload.get("appId") or "") != app_id:
        raise RuntimeError("snapshot appId mismatch")
    if not isinstance(payload.get("generatedAt"), str):
        raise RuntimeError("snapshot generatedAt missing")
    if not isinstance(payload.get("generatedAtEpochMs"), int):
        raise RuntimeError("snapshot generatedAtEpochMs missing")
    if not isinstance(payload.get("payload"), dict):
        raise RuntimeError("snapshot payload missing object")


def _assert_parity_fixture_contains_app(repo_root: Path, app_id: str) -> None:
    parity_path = repo_root / "status_parity" / "status_payload_parity.json"
    if not parity_path.exists():
        raise RuntimeError(f"missing parity fixture: {parity_path}")
    payload = json.loads(parity_path.read_text(encoding="utf-8-sig"))
    if not isinstance(payload, dict) or not isinstance(payload.get(app_id), dict):
        raise RuntimeError(f"parity fixture missing app section: {app_id}")


def _write_spec(repo_path: Path, app_id: str, app_title: str) -> None:
    spec_path = repo_path / "config" / "process-interface" / "status" / f"{app_id}.status.json"
    spec_path.parent.mkdir(parents=True, exist_ok=True)
    spec_path.write_text(
        json.dumps(
            {
                "appId": app_id,
                "appTitle": app_title,
                "runningField": "processRunning",
                "pidField": "processPid",
                "hostRunningField": "processRunning",
                "hostPidField": "processPid",
                "operations": [
                    "_proc=process_running:DbBridge.exe",
                    "processRunning=derive:bool_from_obj:_proc:running:false",
                    "processPid=derive:int_from_obj:_proc:pid",
                    "processPids=derive:json_from_obj:_proc:pids:[]",
                ],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


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
    host_name = "gpi_bridge_host.exe" if is_bridge else "gpi_fixture_host.exe"
    host_path = Path(args.host).resolve() if args.host else (repo_root / "build" / "bin" / host_name)
    target_repo = Path(bridge_repo_text or fixture_repo_text).resolve()
    repo_arg_name = "--bridge-repo" if is_bridge else "--fixture-repo"
    expected_app_id = "bridge" if is_bridge else "40318"

    if not host_path.exists():
        print(f"host binary missing: {host_path}", file=sys.stderr)
        return 2
    if not target_repo.exists():
        print(f"target repo missing: {target_repo}", file=sys.stderr)
        return 2

    ci_surface = subprocess.run(
        [sys.executable, str(repo_root / "scripts" / "ci_status_surface.py")],
        cwd=repo_root,
        text=True,
        capture_output=True,
    )
    if ci_surface.stdout:
        print(ci_surface.stdout, end="")
    if ci_surface.stderr:
        print(ci_surface.stderr, end="", file=sys.stderr)
    if ci_surface.returncode != 0:
        return int(ci_surface.returncode)

    _request(host_path, repo_arg_name, target_repo, "ping", {})
    status = _request(host_path, repo_arg_name, target_repo, "status.get", {"appId": expected_app_id})
    response_payload = status.get("response")
    if not isinstance(response_payload, dict):
        print(f"status response not object: {status}", file=sys.stderr)
        return 2

    config_get = _request(host_path, repo_arg_name, target_repo, "config.get", {"appId": expected_app_id})
    if not isinstance(config_get.get("response"), dict):
        print(f"config.get response not object: {config_get}", file=sys.stderr)
        return 2

    config_set = _request(
        host_path,
        repo_arg_name,
        target_repo,
        "config.set",
        {"appId": expected_app_id, "key": "profile", "value": "sim"},
    )
    if not isinstance(config_set.get("response"), dict):
        print(f"config.set response not object: {config_set}", file=sys.stderr)
        return 2

    action_list = _request(host_path, repo_arg_name, target_repo, "action.list", {"appId": expected_app_id})
    action_list_payload = action_list.get("response")
    if not isinstance(action_list_payload, dict) or not isinstance(action_list_payload.get("actions"), list):
        print(f"action.list response invalid: {action_list}", file=sys.stderr)
        return 2

    invoke_action_name = "config_show"
    found_config_show = False
    actions = action_list_payload.get("actions")
    if isinstance(actions, list):
        index = 0
        while index < len(actions):
            item = actions[index]
            if isinstance(item, dict) and str(item.get("name") or "") == invoke_action_name:
                found_config_show = True
                break
            index += 1
    if not found_config_show:
        print("action.list missing required smoke action 'config_show'", file=sys.stderr)
        return 2

    action_invoke = _request(
        host_path,
        repo_arg_name,
        target_repo,
        "action.invoke",
        {"appId": expected_app_id, "actionName": invoke_action_name, "args": {}},
    )
    if not isinstance(action_invoke.get("response"), dict):
        print(f"action.invoke response invalid: {action_invoke}", file=sys.stderr)
        return 2

    missing_app = _request_raw(host_path, repo_arg_name, target_repo, "config.get", {})
    _assert_error_code(missing_app, "E_BAD_ARG")

    unsupported_app = _request_raw(
        host_path,
        repo_arg_name,
        target_repo,
        "config.get",
        {"appId": "unsupported-app"},
    )
    _assert_error_code(unsupported_app, "E_UNSUPPORTED_APP")

    missing_action_name = _request_raw(
        host_path,
        repo_arg_name,
        target_repo,
        "action.invoke",
        {"appId": expected_app_id, "args": {}},
    )
    _assert_error_code(missing_action_name, "E_BAD_ARG")

    _assert_snapshot_written(target_repo, expected_app_id)
    _assert_parity_fixture_contains_app(repo_root, expected_app_id)

    with tempfile.TemporaryDirectory() as tmp_dir:
        temp_repo = Path(tmp_dir).resolve()
        missing_spec_response = _request_raw(host_path, repo_arg_name, temp_repo, "status.get", {"appId": expected_app_id})
        if bool(missing_spec_response.get("ok")):
            print(f"expected missing spec failure, got: {missing_spec_response}", file=sys.stderr)
            return 2

        _write_spec(temp_repo, expected_app_id, "Smoke App")
        success_response = _request_raw(host_path, repo_arg_name, temp_repo, "status.get", {"appId": expected_app_id})
        if not bool(success_response.get("ok")):
            print(f"expected generated spec success, got: {success_response}", file=sys.stderr)
            return 2

        malformed_path = temp_repo / "config" / "process-interface" / "status" / f"{expected_app_id}.status.json"
        malformed_path.write_text("{bad-json", encoding="utf-8")
        malformed_response = _request_raw(host_path, repo_arg_name, temp_repo, "status.get", {"appId": expected_app_id})
        if bool(malformed_response.get("ok")):
            print(f"expected malformed spec failure, got: {malformed_response}", file=sys.stderr)
            return 2

    print("native smoke ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
