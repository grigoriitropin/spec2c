# Bootstrap, freeze & integrity model

This document describes how `spec2c` anchors its own trust: how the C bootstrap is
frozen, how that freeze is enforced, and how the system recovers. For the language
itself see [`SPEC.md`](SPEC.md); for the governing laws see [`SOUL.md`](SOUL.md).

## The frozen Stage-0 set

`spec2c` is not a single monolithic C file. The bootstrap is a **set of Stage-0 C
files** (currently ~41, listed in `source-code-for-compiler-generation/bootstrap-c-whitelist.txt`)
that implement the compiler and its enforcers. They are the escape hatch: if the IPM
code-generation logic is ever corrupted or produces broken output, the frozen C set
can always be rebuilt with a C compiler to produce a working compiler again.

Each frozen file is pinned by its SHA-256 hash. The hashes are recorded in an
**integrity manifest** (`operator-signed-integrity-manifest-hashes.json`, 56 entries),
and any mutation of a frozen file — even one byte — fails the build on a hash mismatch.

## Why the manifest lives *outside* the enforcer

A checker whose expected hashes are baked into the binary that checks them is a
self-checking checker — theater. So the manifest is a separate, **Ed25519-signed**
artifact (`operator-signed-integrity-manifest-hashes.sig`):

- The **operator** holds the Ed25519 private key, *outside the repository and
  unreachable by the AI*. The enforcer verifies the manifest against an embedded public
  key, fail-closed.
- Regenerating the manifest is therefore an **operator-signed** event, not a casual
  edit. The AI can compute new hashes, but it cannot sign them, so it cannot quietly
  re-freeze a tampered tree.
- The same model protects the **exemption table** (`operator-signed-exemption-name-table.json`):
  the only place non-`.c`/`.ipm` names (e.g. `LICENSE`, `README.md`, `flake.nix`) are
  permitted, and the only place a subtree can be marked scan-excluded — each entry
  default-deny, content-scanned, and covered by its own signature.

The trust anchor always lives outside the artifact it protects.

## Structural limits (enforced at build time)

Applied to every `.c`/`.ipm` file repo-wide (overridable per module only under the same
freeze rules):

| Limit | Value |
|-------|-------|
| Lines per file | 400 |
| Lines per function | 50 |
| Functions per file | 10 |
| Line length | 120 |
| Files per directory | 3 |
| Naming | ≥5 hyphenated words, each ≥3 chars `[a-z0-9]`, ≥1 letter |

Small modules are not a style preference — size is attack surface, and a low ceiling
forces single responsibility and self-documenting names.

## Source layout

```
source-code-for-compiler-generation/   # the compiler and enforcers (Stage-0 C + IPM specs)
  compile-specifications-into-source-code/   # codegen + the structural/security enforcer
  common-support-code-for-spec2c/            # shared runtime primitives (no codegen logic)
  support-code-for-compiled-output/          # support code linked into generated output
  known-answer-tests-for-primitives/         # KATs (e.g. SHA-256 test vectors)
  ipm-compiler-definition-spec-directory/    # the compiler expressed as IPM specifications
modules/                                # early pure-IPM packages (SHA-256, CRC-32, blob store)
```

The scan scope is the **whole repository**, rooted at a path derived at runtime — never
a hardcoded `src/`. Relocating source to escape a narrow scan is treated as an active
evasion, not a workaround.

## Build & recovery

```bash
nix build .#spec2c        # the compiler
nix build .#s2c-enforce    # structural & security enforcer (C)
nix build .#ipm-enforce    # the enforcer expressed in IPM
```

**Recovery principle.** The frozen Stage-0 C set is the recovery floor: because it is
hash-pinned and operator-signed, it can be rebuilt deterministically with a C compiler
independent of the IPM layer. The integrity gate is what guarantees that what you
recover is exactly what was signed, not a tampered copy.

## Deterministic flags

`-O2 -fno-ident -frandom-seed=spec2c -Wl,--build-id=none` — so the same inputs always
produce the same bytes, which is what makes both freezing and tamper-detection
meaningful.
