# Process Interface Contract V0

## Scope
1. This document defines V0 of the shared process interface contract.
2. It is transport-neutral.
3. It is the contract target for:
- `bridge`
- `40318`
- `plc-simulator`
- `ble-simulator`

## Contract Goals
1. Keep GUI layout stable while provider internals evolve.
2. Keep one readable method/data contract for GUI, dev CLI, and app runtimes.
3. Allow incremental migration from script providers to C++ providers without wire breaks.

## Naming Rules
1. Use readable keys only:
- `method`
- `params`
- `response`
- `error`
- `event`
2. Do not use shortened keys like `m`, `p`, `r`, `e`, `evt`.

## Versioning
1. Envelope version key: `interfaceVersion`.
2. Contract identifier key: `interfaceName`.
3. Unknown major `interfaceVersion` must be rejected by clients.

## Transport Model
1. Contract is transport-neutral.
2. Reference binding is newline-delimited JSON over stdio.
3. Each JSON message must be a single line.

## Wire Envelope

### Request
```json
{
  "id": "req-1",
  "method": "status.get",
  "params": {
    "appId": "bridge"
  }
}
```

### Success Response
```json
{
  "id": "req-1",
  "ok": true,
  "response": {}
}
```

### Error Response
```json
{
  "id": "req-1",
  "ok": false,
  "error": {
    "code": "E_BAD_ARG",
    "message": "missing required key: appId",
    "details": {}
  }
}
```

### Event (Optional Push)
```json
{
  "event": "status.changed",
  "params": {
    "appId": "bridge"
  }
}
```

## Required Methods
1. `ping`
2. `status.get`
3. `config.get`
4. `config.set`
5. `action.list`
6. `action.invoke`
7. `action.job.get`

## Optional Methods
1. `events.subscribe`

## Method Contracts

### `ping`
1. Purpose: health check.
2. Params: optional empty object.
3. Response:
```json
{
  "pong": true,
  "interfaceName": "generic-process-interface",
  "interfaceVersion": 1
}
```

### `status.get`
1. Purpose: read current runtime status for one app id.
2. Params:
```json
{
  "appId": "bridge"
}
```
3. Required response fields:
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
4. Additional fields are allowed and app-specific.

### `config.get`
1. Purpose: read config view used by GUI/config tooling.
2. Params:
```json
{
  "appId": "bridge"
}
```
3. Required response fields:
- `repoRoot`
- `valid`
- `errors`
- `entries`
- `paths`
- `configTree`

### `config.set`
1. Purpose: edit config file state for future runs.
2. Params:
```json
{
  "appId": "bridge",
  "key": "transportMode",
  "value": "Com"
}
```
3. Semantics:
- edits file-backed config source
- does not require in-memory immediate mutation
- returns restart marker details
4. Required response fields:
- `changed` (bool)
- `filePath` (string)
- `restartRequired` (bool)
- `pidAtSet` (int|null)
- `message` (string)

### `action.list`
1. Purpose: list invocable actions for one app id.
2. Params:
```json
{
  "appId": "bridge"
}
```
3. Response:
```json
{
  "actions": [
    {
      "name": "run_start",
      "label": "Start",
      "args": []
    }
  ]
}
```

### `action.invoke`
1. Purpose: start action execution.
2. Async by contract.
3. Params:
```json
{
  "appId": "bridge",
  "actionName": "run_start",
  "args": {}
}
```
4. Response:
```json
{
  "jobId": "job-123",
  "state": "queued",
  "acceptedAt": "2026-02-17T16:20:00Z"
}
```

### `action.job.get`
1. Purpose: read async action status.
2. Params:
```json
{
  "appId": "bridge",
  "jobId": "job-123"
}
```
3. Response:
```json
{
  "jobId": "job-123",
  "state": "succeeded",
  "startedAt": "2026-02-17T16:20:00Z",
  "finishedAt": "2026-02-17T16:20:01Z",
  "result": {},
  "stdout": "",
  "stderr": "",
  "error": null
}
```
4. Allowed states:
- `queued`
- `running`
- `succeeded`
- `failed`
- `timeout`
- `canceled`

### `events.subscribe` (Optional)
1. Purpose: subscribe for push events where transport supports long-lived channels.
2. Params:
```json
{
  "topics": [
    "status.changed",
    "log"
  ]
}
```
3. Response:
```json
{
  "subscribed": [
    "status.changed",
    "log"
  ]
}
```

## Error Codes (Minimum)
1. `E_BAD_ARG`
2. `E_UNSUPPORTED_METHOD`
3. `E_UNSUPPORTED_APP`
4. `E_NOT_FOUND`
5. `E_ACTION_FAILED`
6. `E_ACTION_TIMEOUT`
7. `E_CONFIG_INVALID`
8. `E_INTERNAL`

## Compatibility Mapping Rules
1. Legacy op `status` maps to `status.get`.
2. Legacy op `config.show` maps to `config.get`.
3. Legacy op `config.set` maps to `config.set`.
4. Legacy op `actions.list` maps to `action.list`.
5. Legacy op `action.run` maps to `action.invoke` + `action.job.get`.

## App Status Examples

### bridge
```json
{
  "interfaceName": "generic-process-interface",
  "interfaceVersion": 1,
  "appId": "bridge",
  "appTitle": "Test Fixture Bridge",
  "running": false,
  "pid": null,
  "hostRunning": false,
  "hostPid": null,
  "bootId": "",
  "error": ""
}
```

### 40318
```json
{
  "interfaceName": "generic-process-interface",
  "interfaceVersion": 1,
  "appId": "40318",
  "appTitle": "40318",
  "running": false,
  "pid": null,
  "hostRunning": false,
  "hostPid": null,
  "bootId": "",
  "error": ""
}
```

### plc-simulator
```json
{
  "interfaceName": "generic-process-interface",
  "interfaceVersion": 1,
  "appId": "plc-simulator",
  "appTitle": "COM Traffic Simulator (PLC)",
  "running": false,
  "pid": null,
  "hostRunning": false,
  "hostPid": null,
  "bootId": "",
  "error": "",
  "processModel": "hosted-role",
  "roleAppId": "plc-simulator"
}
```

### ble-simulator
```json
{
  "interfaceName": "generic-process-interface",
  "interfaceVersion": 1,
  "appId": "ble-simulator",
  "appTitle": "COM Traffic Simulator (BLE)",
  "running": false,
  "pid": null,
  "hostRunning": false,
  "hostPid": null,
  "bootId": "",
  "error": "",
  "processModel": "hosted-role",
  "roleAppId": "ble-simulator"
}
```

