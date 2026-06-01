# Verification Gate — `spec2c` v0.2 (self-hosting compiler)

> Phase 3 complete. spec2c compiles itself. AST codegen is sole source of truth.
> Stage 0 (spec2c.c) is frozen bootstrap anchor. Runtime (ipm_builtins.c) is 199 LOC primitives.

## Package summary

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `spec2c`                                           |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (Grigorii Tropin + Vehir)             |
| source         | `spec2c.c` (737 LOC, frozen) + `spec2c.ipm` (15 functions, 268 instructions) |
| runtime        | `src/ipm_builtins.c` (199 LOC) + `src/ipm_builtins.h` (57 LOC) |
| build          | `flake.nix` → `nix build .#spec2c`                 |
| ci             | `tests/regression.sh` — 10-gate bootstrap chain    |

## Bootstrap Chain

```
spec2c.c (Stage 0, C, frozen)
  ↓ nix build → reads spec2c.ipm
s1.c (Stage 1, generated)
  ↓ gcc + ipm_builtins.o
stage1 (AST compile_*)
  ↓ reads spec2c.ipm
s2.c (Stage 2)
  ↓ gcc + ipm_builtins.o
stage2 → generates s3.c
  s2.c == s3.c  (source fixpoint)
  stage2 == stage3  (ELF fixpoint)
```

Fixpoint hashes: source `47508527`, ELF `55684e67`.

## SOUL-standard checklist

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | spec2c.c + ipm_builtins.c, cJSON via nixpkgs                |
| 2 | Nix is the only build system        | YES  | `flake.nix`, deterministic flags                            |
| 3 | Fail hard, zero silent failure      | YES  | `json_die()` with function_name, instruction_index, fix_hint|
| 4 | DRY                                 | YES  | Runtime primitives shared; AST compile_* in ipm             |
| 5 | Zero dead code                      | YES  | compile_ast_functions_to_c removed from builtins            |
| 6 | Zero hardcode                       | YES  | Build flags in flake.nix; paths resolved at runtime         |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Both spec2c and generated scaffold compile clean            |
| 8 | No output suppression               | YES  | Errors as JSON to stderr                                    |
| 9 | Security not weakened               | YES  | Bootstrap hatch preserved; fixpoint verified                |
| 10| Self-hosting                        | YES  | AST codegen is sole source of truth; fixpoint proven        |

## Known Debt

- **Indentation drift**: Stage 0 uses depth-tracking, Stage 1+ uses fixed 2-space. Cosmetic only.
- **Number args**: AST compile_function_invocation only handles string args (numbers must be JSON strings). Tracked as `spec2c-#number-args`.

## Verdict

**PASS.** Self-hosting compiler with proven fixpoint. AST is single source of truth.
Runtime is 199 LOC of pure primitives. Bootstrap hatch preserved for recovery.
