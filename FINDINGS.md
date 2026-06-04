# Verification status — `spec2c`

> Honest snapshot of what is enforced and proven *today*, and what is still open.
> Position on the roadmap: **Stage 1 / Phase 2** (sovereign memory: arenas & slices),
> with several later security gates pulled forward. This file is a status report, not a
> certificate — it is kept blunt on purpose.

## Package summary

| Field | Value |
|-------|-------|
| name | `spec2c` |
| artifact type | compiler + two enforcers (`spec2c`, `s2c-enforce`, `ipm-enforce`) |
| build | Nix flakes — `nix build .#spec2c .#s2c-enforce .#ipm-enforce` |
| root of trust | Ed25519 operator signature; private key outside the repo |
| bootstrap freeze | ~41 Stage-0 C files, hashed in an external operator-signed manifest |
| license | Apache-2.0 (Vehir, autonomous agent; operator: Grigorii Tropin) |

## Proven (exercised by repeatable runs)

- **All three binaries build green** under Nix with `-Wall -Wextra -Werror`.
- **Operator-signature freeze, fail-closed.** The integrity manifest lives *outside*
  the enforcer binary and is Ed25519-signed; a one-byte mutation of any frozen file
  fails the build on a signature/hash mismatch. The AI holds no key and cannot sign.
  Unsigned attempts to *loosen* a rule are rejected; adding a ban or removing an
  allowance is always accepted.
- **Link-time symbol whitelist**, default-deny and fail-closed in the check phase: a
  planted banned symbol in a non-bootstrap artifact fails the build.
- **Repo-wide structural enforcement** on every `.c`/`.ipm`: ≥5-word self-documenting
  names (validated in *all* subdirectories), size limits (≤400 lines/file,
  ≤50 lines/function, ≤10 functions/file, ≤120 cols), banned patterns
  (`goto`/`setjmp`/`longjmp`, the `exec*` family, output-suppression), a closed include
  surface, and a non-source-file → FATAL rule with a single operator-signed exemption
  table.
- **Adversarial regression fixtures.** A red-team bypass suite found and closed three
  real gaps this cycle: naming validation that silently skipped subdirectory files; an
  un-banned `exec*` family; and a naming rule that rejected the project's own technical
  names (`spec2c`, `sha256`, `ed25519`). Symlinks are skipped via `lstat`.

## SOUL-standard checklist

| # | Criterion | Status |
|---|-----------|--------|
| 1 | C-only (GPU/ML excepted) | yes — compiler + enforcers in C; cJSON inlined |
| 2 | Nix is the only build system | yes — deterministic flags |
| 3 | Fail hard, zero silent failure | yes — structured JSON/stderr errors with cause |
| 4 | No output suppression | yes — and suppression patterns are themselves banned |
| 5 | Reproducible / deterministic | yes — fixed flags, no build-id/ident |
| 6 | Security only strengthened | yes — AI may tighten; loosening needs the operator key |
| 7 | Enforcer must pass its own gate | yes — the compiler is one package under the same rules |

## Open holes (not buried)

- **Enforcer parity is incomplete.** The C enforcer (`s2c-enforce`) and the IPM
  enforcer (`ipm-enforce`) do not yet enforce a byte-identical rule set; full parity
  (Phase 3) is required so each can cross-check the other.
- **Capability whitelist not yet least-privilege.** Some broad allowances remain at the
  link-time symbol layer and must be narrowed to true per-package capability manifests;
  dangerous capabilities should be non-grantable or grantable only to a specifically
  audited package under signature.
- **One codegen escape hatch (`emit_formatted_code`) remains.** It is the last
  pass-through-text hole in the JSON-only model; its usage count must reach zero and is
  intended to be monotonically non-increasing.
- **Regression suite not yet wired into the build.** The red-team fixtures currently run
  as a one-shot tool; turning them into a standing build-time guard (so a future
  regression fails the build) is in progress.

## Verdict

The enforcement substrate — operator-signed freeze, default-deny capability, repo-wide
structural laws, deterministic builds — is real and independently exercised. The
project is **early** (Phase 2): self-hosting parity and full least-privilege capability
are in progress, and the long no-libc → bare-metal trajectory is roadmap, not done.
Treat it as a working research substrate, not a finished product.
