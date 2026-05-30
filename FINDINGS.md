# Verification Gate — `spec2c` v0.2 (declarative redesign)

> v0.1 was imperative (template strings embedded in C). v0.2 is fully declarative:
> external `skeleton.json` + pure-C section files with `{{key}}` substitution.
> The generator is a thin substitution engine — zero template logic in code.

## Package summary

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `spec2c`                                           |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (Grigorii Tropin)                     |
| source         | `spec2c.c` (270 LOC)                               |
| templates      | `skeleton.json` + `templates/*.c` (53 LOC total)   |
| shared lib     | `lib/vehir_lib.h` + `lib/vehir_lib.c` (180 LOC)    |
| build          | `flake.nix` → `nix build .#spec2c`                 |

## Architecture

```
spec.json → spec2c → tool.c (thin wiring) → calls → vehir_lib (shared logic)
                          ↑                              ├─ vl_die()
  skeleton.json ──────────┤                              ├─ vl_cfg_load/require
  templates/*.c ──────────┘                              ├─ vl_db_check/query_json
                                                         └─ vl_safe_exec()

tool_core.c (hand-written) ← extern → tool.c
```

- **Conditions** are a fixed enum: `always`, `has-config`, `has-db` — not a mini-language.
- **Templates** are pure C + `{{key}}` placeholders — zero logic, zero `{{#if}}`.
- **Core** is REFERENCED (`extern`), never inlined — the hand-written file stands alone.
- **Forbidden patterns** (shell, output suppression, SQL interpolation) are ABSENT from
  templates → structurally impossible in generated tools.

## SOUL-standard checklist

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | spec2c.c + lib/*.c, cJSON via nixpkgs                        |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#spec2c`                          |
| 3 | Fail hard, zero silent failure      | YES  | Every alloc/parse/file-op checked; structured JSON errors    |
| 4 | DRY                                 | YES  | `vl_*` functions shared across all generated tools; `ADD` macro for substitution |
| 5 | Zero dead code                      | YES  | `-Werror` catches all; DB functions stub when not linked     |
| 6 | Zero hardcode                       | YES  | Template paths resolved at runtime via `--base`; spec values via substitution |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Both spec2c and generated scaffold compile clean             |
| 8 | No output suppression               | YES  | Errors to stderr + JSON to stdout; no redirect, no filter    |
| 9 | Security not weakened               | YES  | `vl_safe_exec` = fork+execvp, never shell; `vl_cfg_load` = chmod 600 guard |
| 10| N/A (no legacy, redesigned)         | N/A  | v0.1 discarded; v0.2 is clean architecture                   |
| 11| No hand-rolled parsing              | YES  | cJSON for all JSON parsing                                   |

## LLM-convenience checklist

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | spec2c: `{ok, output}`. Generated tools: `{ok, ...}` via vl_die |
| 2 | Consistent schema                | YES  | Success: `{ok, output}`. Error: `{ok, error}`                  |
| 3 | `--help` / usage                 | YES  | `spec2c --help`; generated tools emit `--help`                 |
| 4 | Predictable exit codes           | YES  | 0 = success, 1 = error, 2 = usage                              |
| 5 | No hidden coupling               | YES  | scaffold → vehir_lib (explicit); core → scaffold (extern usage) |
| 6 | No hidden state                  | YES  | Pure substitution engine; templates are data, not code         |
| 7 | Declarative over imperative       | YES  | skeleton.json + templates = declaration; spec2c = reconciler   |
| 8 | Parseable by an LLM              | YES  | Both spec.json and generated scaffold are LLM-parseable        |

## Proof: file-age regeneration

| Metric                    | Hand-written           | Declarative (v0.2)        |
|---------------------------|------------------------|---------------------------|
| Scaffold (generated)      | —                      | 42 LOC (wiring)           |
| Core (hand-written)       | —                      | 122 LOC (business logic)  |
| Total (scaffold + core)   | 169 LOC (monolith)     | 164 LOC (split)           |
| vehir_lib (shared)        | 0 (inlined in tool)    | 132 LOC (shared by all)   |

Generated scaffold compiles clean, links with vehir_lib + core, and runs:
- `file-age --help` → usage
- `file-age check fresh.json 3600` → `{"ok":true, ...}`
- `file-age check stale.json 3600` → `{"ok":false, ...}`
- `file-age check nonexist.json 3600` → `{"ok":false,"error":"..."}`

## Verdict

**PASS.** The declarative architecture eliminates the imperative template-in-code problem.
The scaffold is correct-by-construction (no forbidden patterns structurally possible).
The shared lib (`vehir_lib`) consolidates cross-cutting logic (DRY at binary level).
The generator is minimal (270 LOC) — templates are data, not code.

## Commands to verify

```bash
nix build .#spec2c                                                    # build generator
./result/bin/spec2c --base . test/file-age.spec.json -o out.c        # generate
cc -Wall -Wextra -Werror -std=c2x -Ilib out.c test/file_age_core.c \
   lib/vehir_lib.c -lcjson -o file-age                               # compile
./file-age check test/data.json 3600 timestamp                        # run
```
