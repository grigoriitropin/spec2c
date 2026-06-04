# AGENTS.md — operating guide for AI agents in this repository

This repository is developed primarily by AI agents under a written constitution.
If you are an AI coding agent (or a human driving one), read this first, then
[`SOUL.md`](SOUL.md) for the full law and [`README.md`](README.md) for the why.

The premise: the architectural and security laws here are enforced by **structure**,
not by convention. You cannot quietly step outside them — a violation fails the build.
Working *inside* the boundary is the path of least resistance; bypassing it is not a
shortcut, it is an architectural violation.

## Build & check

```bash
nix build .#spec2c        # the IPM → C compiler
nix build .#s2c-enforce    # structural & security enforcer (C)
nix build .#ipm-enforce    # the enforcer expressed in IPM
```

A clean build means the enforcers accepted the tree. A violation is a hard, non-zero,
reasoned failure — never a warning. Build before you claim anything works; report
failures with their output.

## Rules the enforcer holds you to

Checked on every `.c`/`.ipm` file, repo-wide. Violations fail the build:

- **English only** — all code, comments, commit messages, and docs.
- **C only** — the sole exception is GPU/ML code that physically cannot be C, behind a
  declared boundary.
- **Naming** — file/identifier names are ≥5 hyphen-separated words, each ≥3 characters
  from `[a-z0-9]`. A binary's name equals its source file's name.
- **Size** — ≤400 lines/file, ≤50 lines/function, ≤10 functions/file, ≤120 columns,
  ≤3 files/directory. Small is the point: size is attack surface.
- **No banned patterns** — `goto`/`setjmp`/`longjmp`, the `exec*` family, output
  suppression (`2>/dev/null`, piping output into `grep`/`head`/`tail`/`sed` to hide it),
  preprocessor pragmas, and similar evasions.
- **Fail hard, zero silent failure** — check every return; surface every error with its
  cause. An empty catch or a swallowed error is a structural violation.
- **Read before write; verify a library/function exists before using it; follow the
  existing patterns; no dead code.**

## Trust model — what you may and may not do

- The freeze manifest and the exemption table are **Ed25519-signed by the human
  operator**, whose private key lives outside the repository. **You do not have the key
  and cannot sign.**
- You may make the rules **stricter** on your own — add a ban, remove an allowance.
- You may **not** loosen anything without an operator signature: widen a capability
  whitelist, add an exemption-table entry, add a new AST node kind, or re-freeze a
  changed file. If a law blocks your task, fix the root cause — do not file the law down.
- **Frozen Stage-0 files must not be mutated.** A one-byte change fails the build on a
  hash/signature mismatch.

## Layout

- `source-code-for-compiler-generation/` — the compiler and enforcers (Stage-0 C + IPM specs).
- `modules/` — early pure-IPM packages (SHA-256, CRC-32, content-addressed blob store).
- `SOUL.md` — the constitution (operating law). `SPEC.md` — the IPM language.
  `bootstrap.md` — the freeze & integrity model. `FINDINGS.md` — honest status.

## When something blocks you

Do not suppress output, widen a gate, or relocate files to escape a scan. Surface the
problem, propose a fix that keeps the law intact, or ask the operator. Silence about a
failure is the one thing the constitution forbids absolutely.
