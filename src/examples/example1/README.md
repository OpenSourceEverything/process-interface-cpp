# Example 1: Host + Client A + Client B (all traffic via host)

This example is intentionally explicit and uses **3 separate binaries**:

1. `gpi_example1_host` (`src/examples/example1/host.cpp`)
2. `gpi_example1_client_a` (`src/examples/example1/client_a.cpp`)
3. `gpi_example1_client_b` (`src/examples/example1/client_b.cpp`)

## What it demonstrates

- `client_a` sends messages to `client_b`
- `client_b` receives those messages by polling the **host**
- `client_b` sends replies back to `client_a` through the **host**
- `client_a` receives replies by polling the **host**
- no client talks directly to the other client

The host acts like a simple local message router/inbox service on TCP localhost.

Default endpoint: `tcp://127.0.0.1:9000`

## Host protocol (JSON over existing IPC request/response)

Host supports these request actions:

- `send`: queue a message for another client
- `poll`: poll your inbox for one message
- `shutdown`: stop the host

This is a simple example protocol built on top of the repo's IPC layer.

## Build

Build the repo (default build includes examples):

```bash
python dev build --clean
```

Or build only the example targets after configure:

```bash
cmake --build artifacts/build --target gpi_example1_host gpi_example1_client_a gpi_example1_client_b
```

## Run (3 terminals)

Terminal 1 (host):

```bash
artifacts/build/bin/gpi_example1_host --endpoint tcp://127.0.0.1:9000
```

Terminal 2 (client B waits for A and replies through host):

```bash
artifacts/build/bin/gpi_example1_client_b --endpoint tcp://127.0.0.1:9000 --count 3
```

Terminal 3 (client A sends to B, waits for replies, then shuts down host):

```bash
artifacts/build/bin/gpi_example1_client_a --endpoint tcp://127.0.0.1:9000 --count 3
```

## What to look for

- Host prints `HOST ROUTE ...` when it queues messages
- Host prints `HOST DELIVER ...` when a client polls and receives a queued message
- `CLIENT_B` prints received messages and sends replies
- `CLIENT_A` prints replies and then sends `shutdown` to stop the host
