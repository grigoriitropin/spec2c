#!/usr/bin/env bash
# CI regression harness — bootstrap chain + fixpoint verification
# Run: ./tests/regression.sh [--verbose]
set -euo pipefail

SPEC2C_IPM="${SPEC2C_IPM:-spec2c.ipm}"
CJSON="${CJSON:-/nix/store/ciwvx39m0iiyr98d1rkc4rl3l9y0wfnd-cjson-1.7.19}"
CFLAGS="-O2 -g0 -fno-ident -fno-asynchronous-unwind-tables -Wl,--build-id=none -frandom-seed=spec2c-bootstrap"
TMPDIR="${TMPDIR:-/tmp/spec2c-regression}"
VERBOSE="${1:-}"

pass()  { echo "  ✓ $1"; }
fail()  { echo "  ✗ $1"; exit 1; }
info()  { [ "$VERBOSE" = "--verbose" ] && echo "  · $1" || true; }

rm -rf "$TMPDIR" && mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

info "=== Step 1: Build Stage 0 (nix) ==="
if ! STORE=$(nix build --no-link --json . 2>/dev/null | python3 -c "import sys,json;print(json.loads(sys.stdin.read())[0]['outputs']['out'])"); then
    fail "nix build failed"
fi
STAGE0="$STORE/bin/spec2c"
pass "Stage 0 built: $STAGE0"

info "=== Step 2: Compile ipm_builtins.o ==="
gcc -c $CFLAGS -Wall -Werror -I"$CJSON/include" src/ipm_builtins.c -o "$TMPDIR/ipm_builtins.o"
pass "ipm_builtins.o"

info "=== Step 3: Stage 0 → s1.c ==="
"$STAGE0" "$SPEC2C_IPM" -o "$TMPDIR/s1.c"
pass "s1.c generated ($(wc -l < "$TMPDIR/s1.c") lines)"

info "=== Step 4: Compile Stage 1 ==="
gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
    -I"$CJSON/include" -I./src \
    "$TMPDIR/s1.c" "$TMPDIR/ipm_builtins.o" \
    -o "$TMPDIR/stage1" -L"$CJSON/lib" -lcjson
pass "Stage 1 compiled"

info "=== Step 5: Stage 1 → s2.c ==="
cp "$SPEC2C_IPM" "$TMPDIR/spec.ipm"
LD_LIBRARY_PATH="$CJSON/lib" "$TMPDIR/stage1" "$TMPDIR/spec.ipm" "$TMPDIR/s2.c"
pass "s2.c generated ($(wc -l < "$TMPDIR/s2.c") lines)"

info "=== Step 6: Compile Stage 2 ==="
cp "$TMPDIR/s2.c" "$TMPDIR/stage_src.c"
gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
    -I"$CJSON/include" -I./src \
    "$TMPDIR/stage_src.c" "$TMPDIR/ipm_builtins.o" \
    -o "$TMPDIR/stage2" -L"$CJSON/lib" -lcjson
pass "Stage 2 compiled"

info "=== Step 7: Stage 2 → s3.c ==="
LD_LIBRARY_PATH="$CJSON/lib" "$TMPDIR/stage2" "$TMPDIR/spec.ipm" "$TMPDIR/s3.c"
pass "s3.c generated ($(wc -l < "$TMPDIR/s3.c") lines)"

info "=== Step 8: Compile Stage 3 ==="
cp "$TMPDIR/s3.c" "$TMPDIR/stage_src.c"
gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
    -I"$CJSON/include" -I./src \
    "$TMPDIR/stage_src.c" "$TMPDIR/ipm_builtins.o" \
    -o "$TMPDIR/stage3" -L"$CJSON/lib" -lcjson
pass "Stage 3 compiled"

info "=== Step 9: Source fixpoint (s2 == s3) ==="
if ! diff "$TMPDIR/s2.c" "$TMPDIR/s3.c" >/dev/null 2>&1; then
    fail "source fixpoint broken: s2 != s3"
fi
S2_HASH=$(sha256sum "$TMPDIR/s2.c" | cut -d' ' -f1)
S3_HASH=$(sha256sum "$TMPDIR/s3.c" | cut -d' ' -f1)
pass "source fixpoint: $S2_HASH"

info "=== Step 10: ELF fixpoint (stage2 == stage3) ==="
strip --strip-all "$TMPDIR/stage2" "$TMPDIR/stage3" 2>/dev/null
if ! diff "$TMPDIR/stage2" "$TMPDIR/stage3" >/dev/null 2>&1; then
    fail "ELF fixpoint broken: stage2 != stage3"
fi
BIN_HASH=$(sha256sum "$TMPDIR/stage2" | cut -d' ' -f1)
pass "ELF fixpoint: $BIN_HASH"

echo ""
echo "=== ALL GATES PASSED ==="
echo "  Source fixpoint: $S2_HASH"
echo "  ELF fixpoint:    $BIN_HASH"
