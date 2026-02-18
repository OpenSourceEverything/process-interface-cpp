#!/usr/bin/env python3
"""Unit-style host contract tests over ZeroMQ IPC."""

from __future__ import annotations

import json
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import Any


def _pick_endpoint() -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = int(sock.getsockname()[1])
    sock.close()
    return f"tcp://127.0.0.1:{port}"


class HostContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = Path(__file__).resolve().parents[2]
        bin_dir = cls.repo_root / "artifacts" / "build" / "bin"
        host_name = "gpi_host.exe" if sys.platform.startswith("win") else "gpi_host"
        client_name = "gpi_client.exe" if sys.platform.startswith("win") else "gpi_client"
        cls.host_path = bin_dir / host_name
        cls.client_path = bin_dir / client_name
        if not cls.host_path.exists():
            raise unittest.SkipTest(f"host binary missing: {cls.host_path}")
        if not cls.client_path.exists():
            raise unittest.SkipTest(f"client binary missing: {cls.client_path}")

    def _request_raw(self, endpoint: str, method: str, params: dict[str, object], override_payload: dict[str, Any] | None = None) -> dict[str, Any]:
        payload = override_payload or {"id": "unit-1", "method": method, "params": params}
        completed = subprocess.run(
            [
                str(self.client_path),
                "--ipc-endpoint",
                endpoint,
                "--request-json",
                json.dumps(payload, separators=(",", ":")),
            ],
            text=True,
            capture_output=True,
            timeout=20.0,
        )
        self.assertEqual(completed.returncode, 0, msg=f"client rc={completed.returncode} stderr={completed.stderr}")
        self.assertTrue((completed.stdout or "").strip(), msg="client returned empty output")
        return json.loads((completed.stdout or "").strip())

    def _request(self, endpoint: str, method: str, params: dict[str, object]) -> dict[str, Any]:
        payload = self._request_raw(endpoint, method, params)
        self.assertTrue(bool(payload.get("ok", False)), msg=str(payload))
        response = payload.get("response")
        self.assertIsInstance(response, dict, msg=str(payload))
        return response

    def _wait_ready(self, endpoint: str) -> None:
        deadline = time.time() + 8.0
        while time.time() < deadline:
            try:
                response = self._request(endpoint, "ping", {})
                if response.get("pong") is True:
                    return
            except Exception:
                pass
            time.sleep(0.15)
        self.fail("host did not become ready")

    def _write_fixture_repo(self, repo_path: Path, app_id: str) -> None:
        status_dir = repo_path / "config" / "process-interface" / "status"
        actions_dir = repo_path / "config" / "actions"
        status_dir.mkdir(parents=True, exist_ok=True)
        actions_dir.mkdir(parents=True, exist_ok=True)

        (repo_path / "config_show.py").write_text(
            "import json\n"
            "print(json.dumps({'repoRoot':'r','valid':True,'errors':[],'entries':{},'paths':{},'configTree':{}}))\n",
            encoding="utf-8",
        )
        (repo_path / "config_set.py").write_text(
            "import json\n"
            "import sys\n"
            "print(json.dumps({'changed':True,'filePath':'cfg.json','restartRequired':False,'pidAtSet':None,'message':sys.argv[1]+'='+sys.argv[2]}))\n",
            encoding="utf-8",
        )
        (repo_path / "run_echo.py").write_text(
            "import json\n"
            "print(json.dumps({'echo':'ok'}))\n",
            encoding="utf-8",
        )

        status_spec = {
            "appId": app_id,
            "appTitle": "Bridge App",
            "runningField": "running",
            "pidField": "pid",
            "hostRunningField": "running",
            "hostPidField": "pid",
            "operations": [
                "running=const:false",
                "pid=const:null",
            ],
        }
        (status_dir / f"{app_id}.status.json").write_text(json.dumps(status_spec, indent=2) + "\n", encoding="utf-8")

        actions = {
            "actions": [
                {
                    "name": "config_show",
                    "label": "Show Config",
                    "cmd": [sys.executable, "config_show.py"],
                    "args": [],
                },
                {
                    "name": "config_set_key",
                    "label": "Set Config",
                    "cmd": [sys.executable, "config_set.py", "{key}", "{value}"],
                    "args": [{"name": "key", "type": "string"}, {"name": "value", "type": "string"}],
                },
                {
                    "name": "run_echo",
                    "label": "Run Echo",
                    "cmd": [sys.executable, "run_echo.py"],
                    "args": [],
                },
            ]
        }
        (actions_dir / f"{app_id}.actions.json").write_text(json.dumps(actions, indent=2) + "\n", encoding="utf-8")

    def _write_profile(self, profile_path: Path, app_id: str) -> None:
        profile = {
            "allowedApps": [app_id],
            "ipc": {"backend": "zmq", "endpoint": "tcp://127.0.0.1:57001"},
            "paths": {
                "statusSpec": "{repoRoot}/config/process-interface/status/{appId}.status.json",
                "statusSnapshot": "{repoRoot}/runtime/custom-status/{appId}.json",
                "actionCatalog": "{repoRoot}/config/actions/{appId}.actions.json",
                "actionJob": "{repoRoot}/runtime/custom-jobs/{appId}/{jobId}.json",
            },
        }
        profile_path.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")

    def _stop_host(self, host: subprocess.Popen[str]) -> None:
        if host.poll() is None:
            host.terminate()
            try:
                host.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait(timeout=5.0)
        if host.stdout:
            host.stdout.close()
        if host.stderr:
            host.stderr.close()

    def test_invalid_profile_missing_allowed_apps_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_path = Path(tmp_dir)
            self._write_fixture_repo(repo_path, "bridge")

            bad_profile = repo_path / "bad.host.json"
            bad_profile.write_text(
                json.dumps(
                    {
                        "ipc": {"backend": "zmq", "endpoint": "tcp://127.0.0.1:57002"},
                        "paths": {
                            "statusSpec": "{repoRoot}/config/process-interface/status/{appId}.status.json",
                            "statusSnapshot": "{repoRoot}/runtime/logs/process-interface/status-source/{appId}.json",
                            "actionCatalog": "{repoRoot}/config/actions/{appId}.actions.json",
                            "actionJob": "{repoRoot}/runtime/state/process-interface/action-jobs/{appId}/{jobId}.json",
                        },
                    }
                ),
                encoding="utf-8",
            )

            endpoint = _pick_endpoint()
            completed = subprocess.run(
                [
                    str(self.host_path),
                    "--repo",
                    str(repo_path),
                    "--host-config",
                    str(bad_profile),
                    "--ipc-endpoint",
                    endpoint,
                ],
                text=True,
                capture_output=True,
                timeout=10.0,
            )
            self.assertNotEqual(completed.returncode, 0)

    def test_invalid_params_shape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_path = Path(tmp_dir)
            app_id = "bridge"
            self._write_fixture_repo(repo_path, app_id)
            profile_path = repo_path / "host.profile.json"
            self._write_profile(profile_path, app_id)

            endpoint = _pick_endpoint()
            host = subprocess.Popen(
                [str(self.host_path), "--repo", str(repo_path), "--host-config", str(profile_path), "--ipc-endpoint", endpoint],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                self._wait_ready(endpoint)
                payload = self._request_raw(
                    endpoint,
                    "ping",
                    {},
                    override_payload={"id": "bad", "method": "ping", "params": []},
                )
                self.assertFalse(bool(payload.get("ok", False)))
                error = payload.get("error") or {}
                self.assertEqual(error.get("code"), "E_BAD_ARG")
            finally:
                self._stop_host(host)

    def test_action_job_roundtrip_and_template_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_path = Path(tmp_dir)
            app_id = "bridge"
            self._write_fixture_repo(repo_path, app_id)
            profile_path = repo_path / "host.profile.json"
            self._write_profile(profile_path, app_id)

            endpoint = _pick_endpoint()
            host = subprocess.Popen(
                [str(self.host_path), "--repo", str(repo_path), "--host-config", str(profile_path), "--ipc-endpoint", endpoint],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                self._wait_ready(endpoint)

                self._request(endpoint, "status.get", {"appId": app_id})
                snapshot_path = repo_path / "runtime" / "custom-status" / f"{app_id}.json"
                self.assertTrue(snapshot_path.exists(), msg=f"missing snapshot path: {snapshot_path}")

                invoke_payload = self._request(endpoint, "action.invoke", {"appId": app_id, "actionName": "run_echo", "args": {}})
                job_id = str(invoke_payload.get("jobId") or "")
                self.assertTrue(job_id)

                job_payload = self._request(endpoint, "action.job.get", {"appId": app_id, "jobId": job_id})
                self.assertEqual(job_payload.get("jobId"), job_id)
                self.assertEqual(job_payload.get("state"), "succeeded")
                self.assertIsInstance(job_payload.get("result"), dict)

                job_file = repo_path / "runtime" / "custom-jobs" / app_id / f"{job_id}.json"
                self.assertTrue(job_file.exists(), msg=f"missing action job path: {job_file}")
            finally:
                self._stop_host(host)


if __name__ == "__main__":
    unittest.main()
