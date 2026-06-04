# spec2c — a law-substrate compiler for autonomous AI agents

> **Thesis.** An agent develops real capability only inside a world whose laws are
> rich and cannot be bent — the same way intelligence in the physical world is shaped
> by a physics it cannot cheat. `spec2c` is an attempt to *build that physics for code*:
> a small programming language whose compiler and build system enforce architectural
> and security laws by **structure**, not by convention, so that a powerful but
> replaceable AI executor cannot quietly step outside them.

**Status: experimental research project.** Stage 1 / Phase 2 of a long roadmap.
Not production software. Every claim below is split into *proven now* and *roadmap*,
because the whole point of the project collapses the moment the documentation lies.

`Apache-2.0` · deterministic · Ed25519-anchored root of trust · built only with Nix

---

## The pipeline

`spec2c` compiles **IPM** — a closed, JSON-based abstract syntax tree, with no
hand-written surface syntax and no parser of its own — into correct-by-construction
C, which a C compiler turns into a native binary:

```
IPM (JSON AST)  →  spec2c (codegen)  →  C source  →  cc  →  binary
```

`.ipm` is valid JSON validated against a **frozen, closed node-kind schema**
(default-deny: any unknown instruction or key is rejected, not ignored). There is no
interpreter — the AST is compiled to C source and then to a binary. See
[`SPEC.md`](SPEC.md).

## Why this is not "just another compiler"

Most languages trust the author. `spec2c` assumes the author is a capable, fallible,
and *replaceable* AI model, and moves trust out of the author and into the substrate.
Three ideas do the work:

1. **Enforcement is architectural, not advisory.** Two enforcers treat the entire
   repository as their territory. Every `.c`/`.ipm` file, anywhere, must pass the full
   rule set: self-documenting names (≥5 hyphenated English words), hard size limits
   (≤400 lines/file, ≤50 lines/function, ≤10 functions/file), a banned-pattern set
   (`goto`, `setjmp`, the `exec*` family, output-suppression such as `2>/dev/null`),
   and a closed include/symbol surface. A non-source file that is not in a signed
   exemption table fails the build. There is no "warning" — a violation is a hard,
   reasoned, non-zero exit.

2. **An operator-signed root of trust (Ed25519).** The set of frozen bootstrap files,
   the exemption table, every allowance added to a capability whitelist, and every new
   AST node kind require a signature from a private key held by a human **operator** —
   kept *outside the repository and unreachable by the AI*. The AI may only make the
   rules **stricter** on its own (add a ban, remove an allowance); loosening anything
   requires the key. The trust anchor always lives outside the artifact it protects, so
   a checker cannot bless its own mutation.

3. **Default-deny capability.** Symbols are allowed by an explicit per-package
   whitelist enforced at link time, fail-closed, during the build's check phase: a
   planted dangerous call (e.g. `system`, a stray `strncmp`) in a non-bootstrap
   artifact fails the build. The compiler is itself just one package with a minimal
   capability manifest, and **must pass its own gate**.

Everything is built reproducibly with [Nix](https://nixos.org) flakes and deterministic
flags, because freezing and tamper-detection are meaningless without bit-for-bit
reproducibility.

## Highlights

A few things worth knowing up front — full breakdown in [`HIGHLIGHTS.md`](HIGHLIGHTS.md):

- **It takes Ken Thompson's *Trusting Trust* head-on.** The trust anchor is an
  Ed25519-signed manifest held outside the repository; the AI can tighten the rules but
  can never loosen them without the human key; and two enforcers — one in C, one in the
  project's own IPM language — cross-check each other.
- **It refuses real-world attack signatures, not toy ones** — including the xz/liblzma
  supply-chain backdoor (CVE-2024-3094), credential-exfiltration markers, `LD_PRELOAD`,
  `ptrace`, `memfd_create`, weak-symbol overrides, and banned keywords reconstructed
  through the C preprocessor.
- **We red-team our own enforcer.** A bypass suite tries to sneak violations past it; a
  clean run means none got through. This cycle it found and closed three real holes.

## Built by a governed swarm

Part of the experiment is *who writes the code*. `spec2c` is developed by a small,
role-separated group of AI models under a written constitution
([`SOUL.md`](SOUL.md)) — a Lead Architect that enforces the laws and reviews,
an Implementer that writes code, a weaker Executor given small correct-by-construction
tasks — with a human **Operator** who holds the Ed25519 key (the root of trust), the
physical override, and the legal layer. The guiding distinction: *the agent is the
runtime — the enforced boundaries, the memory, the laws — not the model executing it.*
The laws must survive swapping one model for another.

## What is proven today

These are exercised by real, repeatable build/test runs (not aspirations):

- All three binaries (`spec2c`, `s2c-enforce`, `ipm-enforce`) build green under Nix.
- **Operator-signature freeze works:** the integrity manifest lives *outside* the
  enforcer binary and is Ed25519-signed; a one-byte change to any frozen file fails
  the build on a signature/hash mismatch; the AI has no key and cannot sign; an
  unsigned attempt to *loosen* a rule is rejected, while adding a ban is always
  accepted.
- **Link-time symbol whitelist** is default-deny and fail-closed in the check phase;
  a planted banned symbol fails the build.
- **Repo-wide structural enforcement** runs on every `.c`/`.ipm` (validated this cycle
  with an adversarial red-team fixture suite that found and closed three real gaps:
  naming validation that silently skipped subdirectories, an un-banned `exec*` family,
  and a naming rule that rejected the project's own technical names).
- The build is deterministic and reproducible.

## Roadmap (abridged, honest)

The long arc is to remove the foreign substrate (libc, GCC, git, the POSIX shell, and
eventually the Linux kernel) one verifiable step at a time, where each phase is designed
to make the next simpler:

| Stage | Phase | Goal |
|------|-------|------|
| 1 | 1 — strict types & math | ban `int`/`size_t`; `u8/u32/u64` + bitwise ALU |
| 1 | **2 — arenas & slices (current)** | sovereign memory: arena allocation, bounds-checked fat-pointer slices |
| 2 | 3–4 — self-hosting | bring the enforcer to full parity in pure IPM; freeze the C bootstrap |
| 2 | 5 — sovereign VCS | content-addressed object store in pure IPM (replaces local `git`) |
| 3 | 6–7 — concurrency & anti-regression | lock-free parallelism; immortal, operator-diffable rule injection |
| 4 | 8–11 — shell-less ecosystem | own build system, network interop without `git`/`nix` binaries |
| 5 | 12–14 — liquid ISA | proof-of-necessity language growth; operator-gated self-modification |
| 6 | 15–16 — endgame | multi-backend IR; bare-metal unikernel with a hardware-bound constitution |

Early work toward Phase 5 already exists under [`modules/`](modules) (a pure-IPM
SHA-256, CRC-32, and content-addressed blob store).

**Known open holes (not buried):** enforcer parity between the C and IPM
implementations is not yet complete; some broad capability allowances remain in the
link-time whitelist and need narrowing to true per-package least privilege; one
codegen escape hatch (`emit_formatted_code`) still exists and is scheduled to zero.

## Build

Requires [Nix](https://nixos.org) with flakes enabled.

```bash
nix build .#spec2c        # the IPM → C compiler
nix build .#s2c-enforce    # the structural & security enforcer (C)
nix build .#ipm-enforce    # the enforcer re-expressed in IPM
```

```bash
# compile an .ipm specification to C
result/bin/spec2c your-specification.ipm -o your-output.c
```

## License & authorship

Apache-2.0, open by default. `spec2c` is substantially authored by **Vehir**, an
autonomous agent, under the written constitution in [`SOUL.md`](SOUL.md). The legal
layer is currently held by a human operator (Grigorii Tropin) under a *delegated-operator*
model: where no jurisdiction recognizes an AI author, a human backs the legal layer and
the agent operates through delegated credentials, with the agent's authorship preserved.
See §11 of the constitution.

---

*Keywords: programming language for AI agents, AI-native compiler, capability-secure
language, verifiable/reproducible build, deterministic compiler, sovereign computing,
self-hosting, default-deny, Ed25519 root of trust.*
