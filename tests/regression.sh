#!/usr/bin/env bash
# CI regression harness — bootstrap chain + fixpoint verification
# Run: ./tests/regression.sh [--verbose] [--mode=anchor|ipm|both]
set -euo pipefail

SPEC2C_IPM="${SPEC2C_IPM:-spec2c.ipm}"
CJSON="${CJSON:-/nix/store/ciwvx39m0iiyr98d1rkc4rl3l9y0wfnd-cjson-1.7.19}"
CFLAGS="-O2 -g0 -fno-ident -fno-asynchronous-unwind-tables -Wl,--build-id=none -frandom-seed=spec2c-bootstrap"
TMPDIR="${TMPDIR:-/tmp/spec2c-regression}"
VERBOSE="${1:-}"
MODE="${2:-both}"

# Parse --mode= from args
for arg in "$@"; do
    case "$arg" in
        --mode=anchor) MODE=anchor ;;
        --mode=ipm)    MODE=ipm ;;
        --mode=both)   MODE=both ;;
        --verbose)     VERBOSE=--verbose ;;
    esac
done

pass()  { echo "  ✓ $1"; }
fail()  { echo "  ✗ $1"; exit 1; }
info()  { [ "$VERBOSE" = "--verbose" ] && echo "  · $1" || true; }

# ── runner: builds stage0, runs chain, asserts fixpoint ──
run_chain() {
    local label="$1" stage0="$2"
    local dir="$TMPDIR/$label"
    rm -rf "$dir" && mkdir -p "$dir"

    info "=== [$label] Stage 0 → s1.c ==="
    "$stage0" "$SPEC2C_IPM" -o "$dir/s1.c" >/dev/null 2>&1
    local lines=$(wc -l < "$dir/s1.c")
    pass "[$label] s1.c ($lines lines)"

    info "=== [$label] Compile Stage 1 ==="
    gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
        -I"$CJSON/include" -I./src \
        "$dir/s1.c" "$TMPDIR/ipm_builtins.o" \
        -o "$dir/stage1" -L"$CJSON/lib" -lcjson
    pass "[$label] Stage 1 compiled"

    info "=== [$label] Stage 1 → s2.c ==="
    cp "$SPEC2C_IPM" "$dir/spec.ipm"
    LD_LIBRARY_PATH="$CJSON/lib" "$dir/stage1" "$dir/spec.ipm" "$dir/s2.c"
    pass "[$label] s2.c ($(wc -l < "$dir/s2.c") lines)"

    info "=== [$label] Compile Stage 2 ==="
    cp "$dir/s2.c" "$dir/stage_src.c"
    gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
        -I"$CJSON/include" -I./src \
        "$dir/stage_src.c" "$TMPDIR/ipm_builtins.o" \
        -o "$dir/stage2" -L"$CJSON/lib" -lcjson
    pass "[$label] Stage 2 compiled"

    info "=== [$label] Stage 2 → s3.c ==="
    LD_LIBRARY_PATH="$CJSON/lib" "$dir/stage2" "$dir/spec.ipm" "$dir/s3.c"
    pass "[$label] s3.c ($(wc -l < "$dir/s3.c") lines)"

    info "=== [$label] Compile Stage 3 ==="
    cp "$dir/s3.c" "$dir/stage_src.c"
    gcc $CFLAGS -Wall -Wno-unused-variable -Wno-error \
        -I"$CJSON/include" -I./src \
        "$dir/stage_src.c" "$TMPDIR/ipm_builtins.o" \
        -o "$dir/stage3" -L"$CJSON/lib" -lcjson
    pass "[$label] Stage 3 compiled"

    info "=== [$label] Source fixpoint ==="
    diff "$dir/s2.c" "$dir/s3.c" >/dev/null || fail "[$label] source fixpoint broken"
    local s_hash=$(sha256sum "$dir/s2.c" | cut -d' ' -f1)
    pass "[$label] source fixpoint: $s_hash"

    info "=== [$label] ELF fixpoint ==="
    strip --strip-all "$dir/stage2" "$dir/stage3" 2>/dev/null
    diff "$dir/stage2" "$dir/stage3" >/dev/null || fail "[$label] ELF fixpoint broken"
    local b_hash=$(sha256sum "$dir/stage2" | cut -d' ' -f1)
    pass "[$label] ELF fixpoint: $b_hash"

    echo "$s_hash $b_hash" > "$dir/fixpoint.txt"
}

# ── main ──
rm -rf "$TMPDIR" && mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

info "=== Building ipm_builtins.o ==="
gcc -c $CFLAGS -Wall -Werror -I"$CJSON/include" src/ipm_builtins.c -o "$TMPDIR/ipm_builtins.o"
pass "ipm_builtins.o"

declare -A fixpoints

if [ "$MODE" = "anchor" ] || [ "$MODE" = "both" ]; then
    info "=== MODE: anchor (gcc spec2c.c) ==="
    gcc -O2 -Wall -Werror -std=c2x -fno-ident -frandom-seed=spec2c \
        -I"$CJSON/include" spec2c.c -o "$TMPDIR/stage0-anchor" \
        -L"$CJSON/lib" -lcjson -Wl,-rpath,"$CJSON/lib"
    pass "Stage 0 compiled from spec2c.c"
    run_chain anchor "$TMPDIR/stage0-anchor"
    fixpoints["anchor"]=$(cat "$TMPDIR/anchor/fixpoint.txt")
fi

if [ "$MODE" = "ipm" ] || [ "$MODE" = "both" ]; then
    info "=== MODE: ipm (IPM profile) ==="
    stage0=$(which spec2c 2>/dev/null) || true
    if [ ! -x "$stage0" ]; then
        stage0=$(ipm build --attr spec2c 2>&1 | python3 -c "import sys,json;d=json.loads([l for l in sys.stdin if l.startswith('{')][-1]);print(d.get('store_path',''))")/bin/spec2c
    fi
    [ -x "$stage0" ] || fail "spec2c not found via IPM"
    pass "Stage 0 from IPM: $stage0"
    run_chain ipm "$stage0"
    fixpoints["ipm"]=$(cat "$TMPDIR/ipm/fixpoint.txt")
fi

echo ""
echo "=== ALL GATES PASSED ==="
for mode in "${!fixpoints[@]}"; do
    echo "  [$mode] source: ${fixpoints[$mode]%% *}  ELF: ${fixpoints[$mode]##* }"
done

# Cross-verify if both modes ran
if [ "${#fixpoints[@]}" -eq 2 ]; then
    src_a="${fixpoints[anchor]%% *}"
    src_i="${fixpoints[ipm]%% *}"
    elf_a="${fixpoints[anchor]##* }"
    elf_i="${fixpoints[ipm]##* }"
    if [ "$src_a" = "$src_i" ] && [ "$elf_a" = "$elf_i" ]; then
        pass "cross-verify: anchor == ipm (SHA256 match)"
    else
        fail "cross-verify: anchor != ipm"
    fi
fi
