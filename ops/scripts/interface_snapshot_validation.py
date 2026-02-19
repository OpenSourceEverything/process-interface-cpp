#!/usr/bin/env python3
"""Validate IPC method surface and publish interface snapshot artifacts."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def detect_apps(repo_root: Path) -> list[str]:
    if (repo_root / "40318-Service.sln").exists():
        return ["40318", "plc-simulator", "ble-simulator"]
    return ["bridge"]


def parse_payload(text: str) -> dict[str, Any]:
    raw = str(text or "").strip()
    if not raw:
        return {}
    payload_raw = json.loads(raw)
    if not isinstance(payload_raw, dict):
        raise RuntimeError("pi_control returned non-object JSON payload")
    return payload_raw


def run_control(repo_root: Path, endpoint: str, timeout_seconds: float, *args: str) -> tuple[int, dict[str, Any], str, str]:
    cmd = [
        sys.executable,
        str(repo_root / "ops" / "scripts" / "pi_control.py"),
        "--endpoint",
        endpoint,
        "--timeout-seconds",
        str(float(timeout_seconds)),
        "--json",
        *args,
    ]
    completed = subprocess.run(cmd, cwd=repo_root, text=True, capture_output=True)
    stdout = str(completed.stdout or "")
    stderr = str(completed.stderr or "")
    payload: dict[str, Any] = {}
    if stdout.strip():
        payload = parse_payload(stdout)
    return int(completed.returncode), payload, stdout, stderr


def require_file(path: Path) -> None:
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")


def publish_interface_snapshot(
    output_root: Path,
    apps: list[str],
    status_payloads: dict[str, dict[str, Any]],
    config_payloads: dict[str, dict[str, Any]],
    action_payloads: dict[str, dict[str, Any]],
) -> None:
    generated_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    manifest = {
        "interfaceName": "generic-process-interface",
        "interfaceVersion": 1,
        "generatedAt": generated_at,
        "apps": list(apps),
        "mode": "online",
    }
    write_json(output_root / "manifest.json", manifest)

    status_snapshot: dict[str, Any] = {"status": {}}
    app_id: str
    for app_id in apps:
        status_payload = dict(status_payloads.get(app_id, {}))
        config_payload = dict(config_payloads.get(app_id, {}))
        actions_payload = dict(action_payloads.get(app_id, {}))

        write_json(output_root / "status" / app_id / "current.json", status_payload)
        status_snapshot["status"][app_id] = status_payload

        write_json(output_root / "config" / app_id / "show.json", config_payload)
        write_json(output_root / "config" / app_id / "effective.json", config_payload)
        entries_payload = config_payload.get("entries")
        write_json(output_root / "config" / app_id / "entries.json", entries_payload if isinstance(entries_payload, list) else [])
        paths_payload = config_payload.get("paths")
        write_json(output_root / "config" / app_id / "paths.json", paths_payload if isinstance(paths_payload, list) else [])
        active_file = output_root / "config" / app_id / "active.txt"
        active_file.parent.mkdir(parents=True, exist_ok=True)
        active_file.write_text("", encoding="utf-8")

        write_json(output_root / "actions" / app_id / "catalog.json", actions_payload)
        inbox_root = output_root / "actions" / app_id / "inbox"
        (inbox_root / "requests").mkdir(parents=True, exist_ok=True)
        (inbox_root / "responses").mkdir(parents=True, exist_ok=True)
        default_request = {"id": "", "actionId": "", "args": {}, "status": "idle"}
        default_response = {"id": "", "actionId": "", "status": "idle", "stdout": "", "stderr": ""}
        write_json(inbox_root / "request.json", default_request)
        write_json(inbox_root / "request.latest.json", default_request)
        write_json(inbox_root / "response.json", default_response)
        write_json(inbox_root / "response.latest.json", default_response)

    write_json(output_root / "status" / "snapshot.json", status_snapshot)


def verify_output_snapshot(output_root: Path, expected_apps: list[str]) -> None:
    manifest = json.loads((output_root / "manifest.json").read_text(encoding="utf-8-sig"))
    if not isinstance(manifest, dict):
        raise RuntimeError("manifest payload must be an object")
    if str(manifest.get("interfaceName") or "") != "generic-process-interface":
        raise RuntimeError("manifest.interfaceName mismatch")
    apps_raw = manifest.get("apps")
    if not isinstance(apps_raw, list):
        raise RuntimeError("manifest.apps missing")
    app_set = {str(item) for item in apps_raw}
    app_id: str
    for app_id in expected_apps:
        if app_id not in app_set:
            raise RuntimeError(f"manifest.apps missing {app_id}")
        require_file(output_root / "status" / app_id / "current.json")
        require_file(output_root / "config" / app_id / "show.json")
        require_file(output_root / "actions" / app_id / "catalog.json")
        require_file(output_root / "actions" / app_id / "inbox" / "request.json")
        require_file(output_root / "actions" / app_id / "inbox" / "response.json")
        require_file(output_root / "actions" / app_id / "inbox" / "requests")
        require_file(output_root / "actions" / app_id / "inbox" / "responses")
    require_file(output_root / "status" / "snapshot.json")


def verify_status_source(status_source_root: Path, expected_apps: list[str]) -> None:
    app_id: str
    for app_id in expected_apps:
        snapshot_path = status_source_root / f"{app_id}.json"
        require_file(snapshot_path)
        payload_raw = json.loads(snapshot_path.read_text(encoding="utf-8-sig"))
        if not isinstance(payload_raw, dict):
            raise RuntimeError(f"status snapshot payload must be object: {snapshot_path}")
        if str(payload_raw.get("appId") or "").strip() != app_id:
            raise RuntimeError(f"{app_id}: status snapshot appId mismatch at {snapshot_path}")
        if not isinstance(payload_raw.get("payload"), dict):
            raise RuntimeError(f"{app_id}: status snapshot payload missing object at {snapshot_path}")


def collect_payloads(
    repo_root: Path,
    endpoint: str,
    timeout_seconds: float,
    apps: list[str],
) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    rc_ping, payload_ping, stdout_ping, stderr_ping = run_control(repo_root, endpoint, timeout_seconds, "ping")
    ping_response = payload_ping.get("response")
    if rc_ping != 0 or not bool(payload_ping.get("ok", False)) or not isinstance(ping_response, dict) or not bool(
        ping_response.get("pong", False)
    ):
        raise RuntimeError(
            "ipc ping failed"
            + (f" stdout={stdout_ping.strip()}" if stdout_ping.strip() else "")
            + (f" stderr={stderr_ping.strip()}" if stderr_ping.strip() else "")
        )

    status_payloads: dict[str, dict[str, Any]] = {}
    config_payloads: dict[str, dict[str, Any]] = {}
    action_payloads: dict[str, dict[str, Any]] = {}

    app_id: str
    for app_id in apps:
        rc_status, payload_status, stdout_status, stderr_status = run_control(
            repo_root,
            endpoint,
            timeout_seconds,
            f"--app={app_id}",
            "status",
        )
        status_response = payload_status.get("response")
        if rc_status != 0 or not isinstance(status_response, dict):
            raise RuntimeError(
                f"{app_id}: ipc status failed"
                + (f" stdout={stdout_status.strip()}" if stdout_status.strip() else "")
                + (f" stderr={stderr_status.strip()}" if stderr_status.strip() else "")
            )
        status_payloads[app_id] = dict(status_response)

        rc_cfg, payload_cfg, stdout_cfg, stderr_cfg = run_control(
            repo_root,
            endpoint,
            timeout_seconds,
            f"--app={app_id}",
            "config-show",
        )
        config_response = payload_cfg.get("response")
        if rc_cfg != 0 or not isinstance(config_response, dict):
            raise RuntimeError(
                f"{app_id}: ipc config show failed"
                + (f" stdout={stdout_cfg.strip()}" if stdout_cfg.strip() else "")
                + (f" stderr={stderr_cfg.strip()}" if stderr_cfg.strip() else "")
            )
        config_payloads[app_id] = dict(config_response)

        rc_actions, payload_actions, stdout_actions, stderr_actions = run_control(
            repo_root,
            endpoint,
            timeout_seconds,
            f"--app={app_id}",
            "actions",
        )
        actions_response = payload_actions.get("response")
        if rc_actions != 0 or not isinstance(actions_response, dict):
            raise RuntimeError(
                f"{app_id}: ipc actions failed"
                + (f" stdout={stdout_actions.strip()}" if stdout_actions.strip() else "")
                + (f" stderr={stderr_actions.strip()}" if stderr_actions.strip() else "")
            )
        action_payloads[app_id] = dict(actions_response)

    return status_payloads, config_payloads, action_payloads


def parse_apps(repo_root: Path, apps_arg: str) -> list[str]:
    values = [part.strip() for part in str(apps_arg or "").split(",") if part.strip()]
    if values:
        return values
    return detect_apps(repo_root)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, help="Target repo root containing ops/scripts/pi_control.py.")
    parser.add_argument("--endpoint", required=True, help="IPC endpoint for already-running gpi_host.")
    parser.add_argument("--output-root", default="runtime/logs/process-interface")
    parser.add_argument(
        "--status-source-root",
        default="",
        help="Directory containing host-written status-source snapshots. Default: <output-root>/status-source.",
    )
    parser.add_argument("--timeout-seconds", type=float, default=20.0)
    parser.add_argument("--apps", default="", help="Comma-separated app ids. Default: auto-detect from repo shape.")
    args = parser.parse_args()

    repo_root = Path(args.repo).resolve()
    output_root = Path(args.output_root)
    if not output_root.is_absolute():
        output_root = (repo_root / output_root).resolve()
    status_source_root = Path(str(args.status_source_root or "").strip() or str(output_root / "status-source"))
    if not status_source_root.is_absolute():
        status_source_root = (repo_root / status_source_root).resolve()

    apps = parse_apps(repo_root, args.apps)
    if output_root.exists():
        shutil.rmtree(output_root, ignore_errors=True)
    output_root.mkdir(parents=True, exist_ok=True)

    status_payloads, config_payloads, action_payloads = collect_payloads(
        repo_root,
        str(args.endpoint),
        float(args.timeout_seconds),
        apps,
    )
    publish_interface_snapshot(
        output_root,
        apps=apps,
        status_payloads=status_payloads,
        config_payloads=config_payloads,
        action_payloads=action_payloads,
    )
    verify_output_snapshot(output_root, apps)
    verify_status_source(status_source_root, apps)
    print("PASS: interface snapshot validation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

