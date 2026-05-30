# Verification Gate — `spec2c` (new package, no legacy)

> This package is net-new — there is no legacy source to compare against. The "ground-truth"
> is the SPEC.md design document and the existing Vehir tool patterns from `~/vehir-next/core/`
> and `~/vehir-next/net/`.

## Package summary

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `spec2c`                                           |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (owned)                               |
| source         | `spec2c.c` (545 LOC)                               |
| build          | `flake.nix` → `nix build .#spec2c`                 |

## What it does

Reads a JSON specification and emits a correct-by-construction Vehir-pattern C skeleton.
The skeleton guarantees: no leaks, no unchecked returns, no suppressed output, no SQL
injection (bind-params via db-proxy), no hardcode, structured JSON output, `--help`.

Generated skeleton sections (conditional on spec):
- `die_json()` — JSON error output with tool name
- Config loader (`default_config_path`, `cfg_load_value`, `cfg_require`) — reads `~/.config/vehir/env`
- DB helpers (`db_check`, `db_query_json`) — wraps `vehir_db_*` API, converts to cJSON
- `usage()` + `main()` — --help, config parsing, DB init, call to hand-written core function

## SOUL-standard checklist

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | Single `.c` file, cJSON via nixpkgs                          |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#spec2c`                          |
| 3 | Fail hard, zero silent failure      | YES  | Every `fopen`, `malloc`, `realloc`, `ferror`, `cJSON_Parse`, `strdup` checked; exits non-zero with structured JSON error |
| 4 | DRY                                 | YES  | `die`/`die_detail` shared error paths; `emit_*` functions reuse `spec_t` |
| 5 | Zero dead code                      | YES  | No unused functions, no ifdefs, no stubs                     |
| 6 | Zero hardcode                       | YES  | Paths/names from JSON spec only; no static assumptions       |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Set in `flake.nix` cflags; builds clean                     |
| 8 | No output suppression               | YES  | Errors to both stderr (human) and stdout (structured JSON)   |
| 9 | Security not weakened               | YES  | No secrets, no network, no privilege escalation; generated code enforces chmod 600 on config |
| 10| N/A (no legacy)                     | N/A  | New package, no legacy to shed                               |
| 11| Hand-rolled parsing replaced        | N/A  | Uses cJSON from nixpkgs for all JSON parsing                 |

## LLM-convenience checklist

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | spec2c output: `{"ok":true/false, ...}`. Generated tools also emit structured JSON |
| 2 | Consistent schema                | YES  | Success: `{ok, output}`. Error: `{ok, error, [detail]}`        |
| 3 | `--help` / usage                 | YES  | `spec2c --help` → clear usage to stderr, exit 0                |
| 4 | Predictable exit codes           | YES  | 0 = success, 1 = error                                          |
| 5 | No hidden coupling               | YES  | Generated code: optional coupling to `db.h` (db package). spec2c itself: standalone |
| 6 | No hidden state                  | YES  | Pure function of (spec file, CLI args)                          |
| 7 | Declarative where it helps       | YES  | JSON spec IS the declarative input; manifest-driven IPM registration |
| 8 | Parseable by an LLM              | YES  | `spec2c spec.json` → C skeleton file + JSON result. Both LLM-parseable |

## Tests performed

| Test                                  | Result | Notes                                                    |
|---------------------------------------|--------|----------------------------------------------------------|
| `spec2c --help`                       | PASS   | Usage to stderr, exit 0                                  |
| `spec2c test/tool.spec.json`          | PASS   | Generated C skeleton with config_keys                    |
| `spec2c - < test/tool.spec.json`      | PASS   | stdin input works                                        |
| `spec2c -o /tmp/out.c test/...`       | PASS   | File output works                                        |
| Minimal spec (name only)              | PASS   | Minimal skeleton, no config, no DB                       |
| Full spec (DB + config + commands)    | PASS   | All sections generated, hyphenated name → C ident        |
| Missing `name` field                  | PASS   | Error: `"spec missing required field"`                   |
| Invalid JSON                          | PASS   | Error: `"JSON parse error in spec"` with detail          |
| Generated skeleton compiles           | PASS   | `cc -Wall -Wextra -Werror -std=c2x -fsyntax-only` clean  |
| Generated skeleton (hyphenated name)  | PASS   | `-` replaced with `_` in C identifiers; compiles clean   |

## Design notes

- **Hyphen → underscore:** Tool names like `data-collector` become `data_collector_run` for C
  identifiers. Display names (error messages, usage) retain hyphens.
- **Config whitespace:** `cfg_load_value` skips whitespace between key and `=` (e.g., `KEY = val`
  matches key `KEY`), matching forge.c behaviour.
- **DB dependency:** Generated code includes `"db.h"` and calls `vehir_db_*` only when the spec
  has `db_queries`. Without DB, the `vehir_db` pointer is set to `NULL` — no db-proxy needed.
- **Core function signature:** Adapts to config/commands presence: `config_path` param only when
  `config_keys` is non-empty; `arg_off` param only when `commands` is non-empty.
- **Config perms guard:** Generated `cfg_load_value` refuses to read config files readable by
  group/other (chmod 600 requirement), matching the Vehir broker security pattern.

## Verdict

**PASS.** `spec2c` meets both SOUL-standard and LLM-convenience requirements. The generated
skeletons match the real Vehir tool patterns observed in `file-age`, `forge`, and `db-probe`.
The tool is net-new with no legacy debt.

## Commands to verify

```bash
nix build .#spec2c                  # builds clean
./result/bin/spec2c --help          # usage
./result/bin/spec2c spec.json       # generate skeleton
cc -Wall -Wextra -Werror -std=c2x   # verify generated code compiles
  -fsyntax-only generated.c -lcjson
```
