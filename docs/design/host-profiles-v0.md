# Host Profiles V0

## Sync 1 Freeze
1. Freeze date: 2026-02-18.
2. These profiles are the stable host launch examples for provider smoke runs.
3. Canonical contract for host/client payloads: `docs/design/contract-v0.md`.

## Bridge Profile Example
Path: `config/hosts/bridge.host.json`

```json
{
  "allowedApps": [
    "bridge"
  ],
  "ipc": {
    "backend": "zmq",
    "endpoint": "tcp://127.0.0.1:57101"
  },
  "paths": {
    "statusSpec": "{repoRoot}/config/process-interface/status/{appId}.status.json",
    "statusSnapshot": "{repoRoot}/runtime/logs/process-interface/status-source/{appId}.json",
    "actionCatalog": "{repoRoot}/config/actions/{appId}.actions.json",
    "actionJob": "{repoRoot}/runtime/state/process-interface/action-jobs/{appId}/{jobId}.json"
  }
}
```

## Fixture Profile Example
Path: `config/hosts/fixture.host.json`

```json
{
  "allowedApps": [
    "40318",
    "plc-simulator",
    "ble-simulator"
  ],
  "ipc": {
    "backend": "zmq",
    "endpoint": "tcp://127.0.0.1:57102"
  },
  "paths": {
    "statusSpec": "{repoRoot}/config/process-interface/status/{appId}.status.json",
    "statusSnapshot": "{repoRoot}/runtime/logs/process-interface/status-source/{appId}.json",
    "actionCatalog": "{repoRoot}/config/actions/{appId}.actions.json",
    "actionJob": "{repoRoot}/runtime/state/process-interface/action-jobs/{appId}/{jobId}.json"
  }
}
```

## Smoke Commands
1. `python ops/scripts/test.py --repo C:/repos/test-fixture-data-bridge --host-config config/hosts/bridge.host.json`
2. `python ops/scripts/test.py --repo Z:/40318-SOFT --host-config config/hosts/fixture.host.json`
