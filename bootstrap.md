# spec2c Bootstrap Architecture

## Immutable Bootstrap Anchor

`spec2c.c` is the Stage 0 compiler — a hand-written C implementation that reads
`.ipm` specifications and generates C source code. It is **frozen and immutable**.
It must never be modified except to fix critical security vulnerabilities or to
align its semantics with the AST codegen (e.g., `""` vs `NULL` fallback).

**Rationale:** If the AST codegen (in `spec2c.ipm`) ever becomes corrupted or
produces broken output, `gcc spec2c.c` must always produce a working Stage 1
binary. This is the bootstrap escape hatch.

## Source of Truth

- `spec2c.ipm` — canonical compiler. All function definitions, all codegen logic.
- `spec2c.c` — bootstrap loader. Frozen C implementation.
- `src/ipm_builtins.c` — runtime primitives. No compiler logic.

## Bootstrap Chain

```
Stage 0 (spec2c.c, frozen)
  ↓ reads spec2c.ipm
Stage 1 (s1.c, generated)
  ↓ uses AST compile_* functions
Stage 2 (s2.c, generated)
  ↓ fixed point
Stage 3 (s3.c, generated)
  s2.c == s3.c  (source fixpoint)
  stage2 == stage3  (ELF fixpoint)
```

## Known Drift

Stage 0 and Stage 1+ produce functionally equivalent but not byte-identical C:
- Stage 0 uses depth-tracking indentation
- Stage 1+ uses fixed 2-space indentation

This is a cosmetic formatting legacy. It does not affect correctness or ELF identity.

## CI Regression

```bash
./tests/regression.sh
```

Runs the full bootstrap chain and asserts:
- Source fixpoint: s2.c == s3.c
- ELF fixpoint: stage2 == stage3

## Deterministic Build Flags

Defined in `flake.nix`:
- `-O2 -fno-ident -frandom-seed=spec2c -Wl,--build-id=none`

## Frozen Boundaries

```
Core language:      8 instruction types (immutable)
Core runtime:       src/ipm_builtins.c (199 LOC, immutable)
Bootstrap anchor:   spec2c.c at commit 446c083 (re-frozen)
Self-hosting loop:  spec2c.ipm + modules/ (no optional builtins)

Expansion zone (additive only):
  Optional runtime: runtime/ipm_builtins_fs.c (≤150 LOC, pure wrappers)
  New .ipm programs: standalone workspaces, not IPM internals
```

## Bootstrap Recovery

If the multi-file compiler is corrupted, restore from the monolithic bootstrap:
```bash
gcc spec2c.c -o stage0_recovery
./stage0_recovery bootstrap/spec2c-monolith.ipm -o recovery.c
gcc recovery.c src/ipm_builtins.c -o recovery
./recovery modules/codegen.ipm codegen.c  # works with multi-file modules
```

## Structural Enforcement

spec2c enforces at compile time (defaults, overridable per-module via `structural_limits`):
- File lines: max 2000
- Functions per file: max 15
- Top-level instructions per function: max 250
