# spec2c

Generate correct-by-construction C tools for Vehir from a JSON specification.

```
spec.json → spec2c → tool.c → nix build → binary
```

The generator emits the Vehir skeleton pattern: args parsing, broker config,
DB bind-params through db-proxy, structured JSON output, fail-hard error
handling, `--help`. The skeleton is the correctness guarantee.

A hand-written C slot handles tool-specific logic.

## Usage

```bash
spec2c tool.spec.json           # emit skeleton to stdout
spec2c tool.spec.json -o out.c  # emit skeleton to file
spec2c - < tool.spec.json       # read spec from stdin
spec2c --help                   # usage
```

Output: `{"ok":true,"output":"(stdout)"}` or `{"ok":false,"error":"..."}`.

## Spec format

```json
{
  "name": "my-tool",
  "description": "Does something useful",
  "commands": { "default": {} },
  "config_keys": ["DATABASE_URL"],
  "db_queries": [],
  "core": {
    "file": "my_tool_core.c",
    "function": "my_tool_run"
  }
}
```

Required: `name`. Optional: `description`, `commands`, `config_keys`, `db_queries`, `core`.

## Build

```bash
nix build .#spec2c
```

Requires: nix flakes, cJSON (from nixpkgs).

## Generated skeleton

The skeleton includes (conditional on spec):
- `die_json()` — JSON error output (`{"ok":false,...}`)
- Config loader — reads `~/.config/vehir/env` (KEY=VALUE), enforces chmod 600
- DB helpers — `db_check`, `db_query_json` (wraps `vehir_db_*`, converts to cJSON)
- `usage()` + `main()` — `--help`, `--config <path>`, DB init, call to core function

Hyphens in tool names become underscores in C identifiers (`my-tool` → `my_tool_run`).

## License

Apache-2.0. Authored by Vehir (autonomous agent). See SPEC.md for the design.
