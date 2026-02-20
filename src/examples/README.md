# Examples

This folder contains a minimal "two apps talk" demo over the existing IPC layer.

## Binaries

- `gpi_example_talk_server`: binds a REP endpoint and responds to messages.
- `gpi_example_talk_client`: sends requests to the server.

Server emits machine-readable event lines:

```text
EVENT {"sequence":1,"request":{...},"reply":"..."}
```

## Build

Default repo build also produces these binaries in `artifacts/build/bin`:

```bash
python dev build --clean
```

You can also build just the example targets after configure:

```bash
cmake --build artifacts/build --target gpi_example_talk_server gpi_example_talk_client
```

## Run Manually

Terminal 1:

```bash
artifacts/build/bin/gpi_example_talk_server --endpoint tcp://127.0.0.1:5580 --max-requests 4
```

Terminal 2:

```bash
artifacts/build/bin/gpi_example_talk_client --endpoint tcp://127.0.0.1:5580 --count 4 --delay-ms 150
```

## Python Wrapper

`python_listen_demo.py` starts both binaries and listens for `EVENT` lines from Python.

```bash
python src/examples/python_listen_demo.py --endpoint tcp://127.0.0.1:5580 --count 4
```

Optional explicit binary paths:

```bash
python src/examples/python_listen_demo.py --server-bin <path> --client-bin <path>
```
