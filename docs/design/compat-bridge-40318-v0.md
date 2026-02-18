# Bridge + 40318 Compatibility Mapping to V0

## Purpose
1. Map current control-plane behavior to V0 contract methods.
2. Identify adapter behavior needed during migration.
3. Keep GUI and dev-cli behavior stable while backend shifts to C++ implementation.

## Current Operations

### Bridge (`C:/repos/test-fixture-data-bridge`)
1. Provider: `ops/scripts/interface_provider.py`
2. IPC server: `ops/scripts/status_ipc_server.py`
3. Filesystem sync: `ops/scripts/interface_fs_sync.py`
4. Current IPC ops:
- `ping`
- `status`
- `actions.list`
- `action.run`
- `config.show`
- `config.set`

### Fixture (`Z:/40318-SOFT`)
1. Provider: `ops/scripts/interface_provider.py`
2. IPC server: `ops/scripts/status_ipc_server.py`
3. Filesystem sync: `ops/scripts/interface_fs_sync.py`
4. Current IPC ops:
- `ping`
- `status`
- `actions.list`
- `action.run`
- `config.show`
- `config.set`

## V0 Method Mapping

| Current op | V0 method | Migration rule |
|---|---|---|
| `ping` | `ping` | direct map |
| `status` | `status.get` | add `params.appId` |
| `config.show` | `config.get` | direct map |
| `config.set` | `config.set` | direct map with restart metadata |
| `actions.list` | `action.list` | direct map |
| `action.run` | `action.invoke` + `action.job.get` | wrap sync execution into async job contract |

## Async Action Bridging Rule
1. If backend is sync today:
- `action.invoke` creates `jobId`.
- provider executes action immediately.
- store terminal job record (`succeeded` or `failed`).
- return accepted `jobId`.
2. `action.job.get` returns stored terminal result.
3. This keeps GUI contract async without forcing runtime threading redesign in first migration step.

## Status Field Compatibility
1. V0 required base fields already exist in current provider payloads:
- `interfaceName`
- `interfaceVersion`
- `appId`
- `appTitle`
- `running`
- `pid`
- `hostRunning`
- `hostPid`
- `bootId`
- `error`
2. Hosted role fields remain additive:
- `processModel`
- `roleAppId`

## Config Compatibility
1. `config.get` response remains:
- `repoRoot`
- `valid`
- `errors`
- `entries`
- `paths`
- `configTree`
2. `config.set` must include restart result fields:
- `changed`
- `filePath`
- `restartRequired`
- `pidAtSet`
- `message`

## Filesystem Snapshot Compatibility
1. Keep snapshot folders under artifacts:
- `artifacts/snapshots/interface/status/<app>/...`
- `artifacts/snapshots/interface/config/<app>/...`
- `artifacts/snapshots/interface/actions/<app>/...`
2. FS sync remains an adapter over the same provider methods.
3. Do not change GUI target layout during contract migration.

## Adapter Checklist
1. Add V0 method aliases in IPC server while keeping legacy op names temporarily.
2. Add async job table in provider for `action.invoke` / `action.job.get`.
3. Keep existing payload keys for GUI widgets.
4. Preserve parity gates:
- `ci_interface_fs.py`
- `ci_interface_ipc.py`
- `ci_interface_control.py`
- `ci_interface_parity.py`

## Exit Criteria
1. Both repos support V0 method names.
2. Legacy op names can be served as compatibility aliases.
3. Parity checks remain green across `bridge`, `40318`, `plc-simulator`, `ble-simulator`.
