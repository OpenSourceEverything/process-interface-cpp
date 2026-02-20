# process-interface-cpp

## Layout
- `src/`: C++ source modules and host entrypoints.
- `ops/scripts/`: Python build/test/guard scripts.
- `ops/tools/`: Operational tooling.
- `test/`: Test fixtures and parity assets.
- `runtime/`: Active runtime state and logs.
- `artifacts/`: Build outputs and retained artifacts.
- `config/`: Repository-level config inputs.
- `db/`: Database and migration assets.
- `external/`: Reserved for external dependencies/submodules.
- `docs/`: Design and contract docs.

## Commands
- Init deps: `git submodule update --init --recursive`
- Build: `python dev build --clean`
- Build (Debug): `python dev build --clean --config Debug`
- Guardrails: `python ops/scripts/ci_status_surface.py`
- Smoke test: `python ops/scripts/test.py --repo <repo> --host-config <profile>`

## Runtime vs Artifacts
- Active status snapshots/logs: `runtime/logs/...`
- Build outputs: `artifacts/build/...`

## Host Launch
- Binary: `artifacts/build/bin/gpi_host.exe`
- Client: `artifacts/build/bin/gpi_client.exe`
- Launch contract: `gpi_host --repo <target-repo> --host-config <host-profile.json> [--ipc-endpoint tcp://127.0.0.1:<port>]`
- Frozen contract: `docs/design/contract-v0.md`
- Frozen host profile examples: `docs/design/host-profiles-v0.md`

## Examples
- Example sources: `src/examples/`
- Example docs: `src/examples/README.md`
