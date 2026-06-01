# spec2c — Self-Hosting .ipm Compiler

Declarative, deterministic, self-reproducing compiler. Converts `.ipm` specifications
(JSON with 8 instruction types) to correct-by-construction C source code.

## Architecture

```
spec2c.ipm (AST, 15 functions, 268 instructions)
     ↓ compiled by
spec2c.c (Stage 0, frozen C bootstrap)
     ↓ produces
stage1.c (generated, uses AST compile_*)
     ↓ compiled to
stage1 binary → generates stage2.c
     ↓
s2.c == s3.c  (source fixpoint)
stage2 == stage3  (ELF fixpoint)
```

- **Stage 0**: `spec2c.c` — frozen C implementation. Bootstrap escape hatch.
- **Stage 1+**: `spec2c.ipm` — canonical compiler. AST codegen is sole source of truth.
- **Runtime**: `src/ipm_builtins.c` (199 LOC) — I/O, JSON, hash table, string buffer. Zero compiler logic.

## .ipm Language

8 instruction types (frozen):
- `variable_declaration` — typed variable from JSON field
- `function_invocation` — call builtin or AST function
- `conditional_branch` — if/else on key_exists, is_not_null, string_equals
- `return_statement` — return value or status
- `iterate_over_collection` — for-loop over JSON array
- `iterate_over_object_keys` — foreach over JSON object keys
- `access_json_field` — read field from cJSON object
- `database_execute_parameterized` — stub (no DB dependency)

All source code: `spec2c.ipm`. Schema: `ipm_schema.json`.

## Usage

```bash
# Build Stage 0
nix build .#spec2c

# Generate C from .ipm
result/bin/spec2c tool.ipm -o tool.c

# Run CI harness (full bootstrap chain)
./tests/regression.sh
```

## Bootstrap Guarantees

| Gate | Status |
|------|--------|
| Source fixpoint: s2 == s3 == s4 | sha256 match |
| ELF fixpoint: stage2 == stage3 | sha256 match |
| AST codegen: sole source of truth | 268 instructions, zero C leak |
| Bootstrap escape hatch | spec2c.c frozen, always works |

## Build

```bash
nix build .#spec2c        # Stage 0 binary
./tests/regression.sh      # Verify self-hosting chain
```

Deterministic flags: `-O2 -fno-ident -frandom-seed=spec2c -Wl,--build-id=none`

## License

Apache-2.0. Authors: Vehir (AST compiler), Grigorii Tropin (C bootstrap).
