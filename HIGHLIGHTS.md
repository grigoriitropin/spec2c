# Highlights — the interesting parts

This is the honest deep-dive behind [`README.md`](README.md): the mechanisms that make
`spec2c` more than a JSON-to-C compiler, and the catalog of real-world attack classes it
is built to refuse. Every item is tagged **[built]** (exercised by real build/test runs)
or **[design]** (specified on the roadmap, not yet implemented). The split is the point —
a law-substrate whose documentation overclaims has already failed.

The thesis: an agent gains real capability only inside a world whose laws are rich and
cannot be bent. So we are building a *physics for code* — laws enforced by structure, not
by trust in the author — because the author here is a powerful, fallible, replaceable AI.

---

## 1. The hard problem: Trusting Trust

Ken Thompson's 1984 *Reflections on Trusting Trust* is the deepest attack on any compiler:
a compiler can be taught to inject a backdoor into everything it builds — **including a
fresh copy of itself** — so the backdoor survives even when the source is clean and
re-audited. You cannot find it by reading the source, because the malice lives in the
binary that compiles the source.

This is not academic here. `spec2c` is a compiler **written largely by an AI**, which is
itself a replaceable executor. A careless or hostile model that could hide malice in the
substrate would have built the perfect Thompson compiler. So the project's security model
is, at its core, a layered answer to Thompson:

- **[built] The trust anchor lives outside the artifact it protects.** The set of frozen
  bootstrap files is pinned by SHA-256 in an external, **Ed25519-signed** manifest. A
  checker whose expected hashes are baked into the binary that checks them is a
  self-checking checker — theater. Ours is verified against a key held by a human
  *outside the repository*. The AI has no key and cannot re-bless a tampered tree.
- **[built] The asymmetry.** The AI may make the rules *stricter* on its own — add a ban,
  remove an allowance — but can **never loosen** one (widen a whitelist, add an exemption,
  add an AST node kind, re-freeze a changed file) without the operator's signature. A
  compiler that can tighten its own cage but not open it.
- **[built] Two enforcers check each other.** The rules exist twice — once in C
  (`s2c-enforce`), once in the project's own IPM language (`ipm-enforce`) — built from
  independent sources toward identical verdicts. Divergence is itself a failure. This is
  the answer to "who checks the checker": they check each other.
- **[built] Determinism.** Reproducible Nix builds with fixed flags mean the same inputs
  always produce the same bytes — the precondition for any of the above to detect
  tampering at all.
- **[design] Diverse Double-Compiling (the classic Thompson countermeasure).** Security
  primitives (crypto, codegen) enter only by *reproducing an audited reference* pinned at
  a frozen hash, with a permanent diff gate in CI — so a synthesized backdoor cannot
  survive a rebuild from an independent source.
- **[design] Operator-diffable immortal rules (the meta-Thompson trap).** A later phase
  injects the rule-set into every compiled artifact even if the visible source is stripped
  — but the injector must be **operator-diffable** via an offline, signed oracle that emits
  and hashes the injected payload. Otherwise the immortal payload would itself become the
  perfect Thompson backdoor. Immortality is only safe if it stays inspectable.

## 2. The mechanisms (a dozen and counting)

**Root of trust & freeze**
- **[built]** Ed25519 operator-signed external manifest; one-byte change to any of ~41
  Stage-0 files fails the build on a hash/signature mismatch.
- **[built]** SHA-256 known-answer tests (NIST vectors) wired into the build's check phase
  — the hash primitive is verified before anything is trusted to it.

**Default-deny capability**
- **[built]** Link-time symbol whitelist with our **own ELF reader**: it opens the binary,
  reads the `Elf64` headers, and walks `SHT_DYNSYM`/`SHT_SYMTAB` directly — *no
  `readelf | grep`*, because a filtering pipe is itself a banned blindness. Fail-closed: a
  planted dangerous symbol fails the build.
- **[built]** Weak-symbol (`STB_WEAK`) ban with a small explicit permit list — closes the
  "silently override `malloc`/`memcpy`" trick.
- **[built]** Closed, default-deny AST schema: any unknown instruction kind or key in an
  `.ipm` file is rejected, not ignored. "Valid JSON" is not enough.
- **[design]** Per-package capability manifests (least privilege): each package declares
  only the syscalls it needs; raw `syscall`, `exec`, `dlopen` are non-grantable or
  grantable only to a specifically audited package under signature. A name-ban is defeated
  by a raw syscall *number*, so the syscall layer becomes a default-deny whitelist of
  numbers, not a blacklist of names.

**Structural law (repo-wide)**
- **[built]** Sovereign-root scan: scope is the whole repository, rooted at a path derived
  at runtime, never a hardcoded `src/`. Relocating source to escape a narrow scan is
  treated as an active evasion.
- **[built]** Self-documenting naming as a byte-level DFA (no POSIX regex): ≥5 hyphenated
  words, each ≥3 chars `[a-z0-9]`. Validated in *every* subdirectory.
- **[built]** Hard size limits (≤400 lines/file, ≤50/function, ≤10 functions/file,
  ≤120 cols, ≤3 files/dir) — size is attack surface.
- **[built]** Non-source file → FATAL, with a single Ed25519-signed, content-scanned,
  default-deny exemption table (and a `subtree-skip` directive for test fixtures).

**Anti-obfuscation**
- **[built]** Preprocessor-expansion scan: the enforcer runs `cc -E` and re-scans, so a
  banned keyword reconstructed via `##` token-paste or `_Pragma` is caught even when it is
  invisible in the raw source.
- **[built]** Unicode-escape decode: `goto` in JSON is decoded and
  recognized as `goto`.
- **[built]** Output-suppression ban: `2>/dev/null`, piping output into
  `grep`/`head`/`tail`/`sed` to hide it, always-true `|| true` clauses — silence about a
  failure is the one thing forbidden absolutely.

**Robustness**
- **[built]** Symlinks and FIFOs are skipped via `lstat` (no following a symlink to an
  out-of-tree target); a recursion-depth limit (256) refuses pathologically deep trees.
- **[built]** `cJSON` is statically inlined (zero undefined `cJSON_*` symbols) — no
  dynamic-linking surface.
- **[built]** A generated-code mode enforces *security* on machine-generated C while
  exempting human-maintainability rules — so codegen output is checked for danger without
  being penalized for being long.

**Methodology**
- **[built]** An adversarial red-team regression suite: bypass attempts are written *for*
  the enforcer, and a clean run means none of them got through. This cycle it found and
  closed three real holes (see [`FINDINGS.md`](FINDINGS.md)).

## 3. The adversarial catalog — the attack classes we refuse

A sample of the modern techniques the rule set is built against. Many are concrete entries
in the banned-pattern set; the build fails if they appear in a non-exempt artifact.

| Attack class | Defense | Status |
|--------------|---------|--------|
| Trusting Trust (self-perpetuating compiler backdoor) | external signed anchor + dual enforcer + reproduce-1:1 | [built]/[design] |
| Self-checking checker (hashes baked into the checker) | manifest signed by an outside key | [built] |
| **Supply-chain backdoor** (xz/liblzma, CVE-2024-3094) | banned signatures: `build-to-host.m4`, `good-large_compressed` | [built] |
| **Secret/credential exfiltration** | banned markers: `NPM_TOKEN`, `GITHUB_TOKEN`, `ACTIONS_ID_TOKEN`, `bypass_2fa` | [built] |
| `LD_PRELOAD` shared-object injection | banned pattern | [built] |
| `ptrace` process injection | banned pattern | [built] |
| `dlopen`/`dlsym` runtime arbitrary code | banned pattern + symbol whitelist | [built] |
| `memfd_create` fileless execution | banned pattern | [built] |
| `process_vm_readv`/`shmat`/`shmget` cross-process memory | banned patterns | [built] |
| `inotify`/`fanotify` surveillance | banned patterns | [built] |
| `capset`/`seccomp` privilege/sandbox tampering | banned patterns | [built] |
| `int 0x80` legacy syscall gate | banned pattern | [built] |
| `__attribute__((constructor))` code before `main` | caught | [built] |
| `STB_WEAK` symbol override | weak-symbol ban + permit list | [built] |
| `exec*` family (spawn) | banned pattern | [built] |
| `##` / `_Pragma` keyword reconstruction | preprocessor-expansion scan | [built] |
| Unicode-escape obfuscation | JSON decode + content scan | [built] |
| `base64`/`rot13`/`xor` payload obfuscation | banned markers | [built] |
| Output suppression (`2>/dev/null`, hiding pipes) | banned pattern | [built] |
| File-relocation evasion | whole-repo runtime-rooted scan | [built] |
| Hidden dotfiles / `LICENSE.c` exemption-as-directory | naming DFA + exact-match exemptions | [built] |
| Symlink-to-infected-target | `lstat` skip | [built] |
| Deep-recursion resource exhaustion | depth limit (256) | [built] |
| Raw syscall *number* defeating a name-ban | syscall-number default-deny whitelist | [design] |
| Harvest-Now-Decrypt-Later | hybrid X25519 + ML-KEM-768 at the network layer | [design] |

## 4. Built by a governed swarm

`spec2c` is developed by role-separated AI models under a written constitution
([`SOUL.md`](SOUL.md)) — Architect, Implementer, Executor — with a human Operator holding
the Ed25519 key, the physical override, and the legal layer. The guiding distinction:
*the agent is the runtime — the enforced boundaries, the memory, the laws — not the model
executing it.* The laws must survive a model swap.

## 5. Honest status

Stage 1 / Phase 2 of a long roadmap. Open holes, not buried: enforcer parity between the C
and IPM implementations is incomplete; some capability allowances still need narrowing to
true per-package least privilege; one codegen escape hatch (`emit_formatted_code`) remains;
the red-team suite is not yet wired into the build as a standing gate. The long arc —
removing libc, GCC, git, the shell, and eventually the kernel, ending at a bare-metal
unikernel whose constitution is bound to the hardware — is roadmap, not done. Treat this as
a working research substrate, not a finished product.
