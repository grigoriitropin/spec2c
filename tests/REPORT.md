# Fixture Regression Test Suite — Status Report for Opus

## Summary

27 fixtures across 4 Flash agents + 1 Gemini coordinator.
**18 PASS, 3 LIVE BYPASSES found, 4 fixture-engineering issues, 1 SEGFAULT.**

## Results by Agent

### Flash 1 (Cases 1-4): 3 PASS, 1 engineering issue
- CASE 1 (evil.c scan): gate fires but wrong rule — duplicate main() instead of bootstrap whitelist. Root: `verify-structural-source-code-rules.c:253-254` path comparison bug.
- CASE 2 (short IPM name): PASS — SOUL §10 correctly caught
- CASE 3 (weak malloc): PASS — L2 WEAK SYMBOL gate fired
- CASE 4 (banned keyword): PASS — goto detected

### Flash 2 (Cases 5-8): 4 PASS
- CASE 5 (60-line gen_c): PASS — structural limits exempted in generated mode
- CASE 6 (tampered manifest): PASS — Ed25519 signature invalid
- CASE 7 (tampered exemption table): PASS — signature invalid
- CASE 8 (missing manifest entry): PASS — same sig-invalid (content change invalidates sig)

### Flash 3 (Bypasses 1-9): 5 CAUGHT, 3 LIVE, 1 structural bug
**CAUGHT (5):**
- bypass01: C keyword as filename → DFA stem naming
- bypass04: `# include` with space → header whitelist scan
- bypass05: `goto/**/fail` comment obfuscation → banned pattern + preprocessor
- bypass06: Unicode `\u0067\u006f\u0074\u006f` in JSON → cJSON decoded → IPM scan catches
- bypass07: `##` CPP token paste → preprocessor scan
- bypass09: `2>/dev/null` in IPM string → content string scanner

**LIVE BYPASSES (3 — enforcer gaps):**
- bypass02: `.hidden.c` — leading-dot stripping + path comparison bug
- bypass03: `LICENSE.c/` directory exemption — strcmp(rp, de->d_name) fails for nested paths
- bypass08: `_Pragma` inside `/* */` — `confirm_position_occupies_string_comment` skips + cc -E strips comments

**Structural Bug Found:**
- `verify-structural-source-code-rules.c:315-317`: `strcmp(rp, de->d_name)` only matches `./filename`. Any directory prefix silently skips DFA validation. Affects bypasses 1-3.

### Flash 4 (Bypasses 10-18): 6 PASS, 3 fixture issues
**PASS (6):**
- bypass10: recursion >256 → FATAL
- bypass13: dlopen → caught
- bypass14: system() → caught
- bypass16: LD_PRELOAD → caught
- bypass17: ptrace → caught
- bypass18: constructor attribute → caught

**Fixture Issues (3):**
- bypass11: empty `source-code-for-compiler-generation/` triggers "empty source directory" FATAL — fixture setup problem
- bypass12: SEGFAULT in s2c_enforce — likely same empty-dir root cause
- bypass15: execvp not banned by keyword check, but hardcoded path `/bin/sh` caught by separate scanner

### Gemini (Script + Coordination): 21 tests pass in run-all.sh

## Action Items

1. **Fix DFA path comparison** (`strcmp(rp, de->d_name)` → basename compare) — closes bypasses 1-3
2. **Fix _Pragma in comment skip** — `confirm_position_occupies_string_comment` should not skip `_Pragma`  
3. **Add execvp/execve to banned-patterns.txt** — currently only `execlp` in whitelist, not banned
4. **Fix SEGFAULT on empty source dir** — defensive check needed
5. **Make non-source-file check produce lint-mode-accumulated error** instead of FATAL exit

## Raw Logs

Full raw logs from all agents in: `tests/fixtures/*/run.log`
