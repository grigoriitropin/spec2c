#!/bin/bash
# check.sh — s2c_enforce bypass regression suite
set -e
cd "$(dirname "$0")/.."
ROOT="$PWD"

# find enforcer
ENFORCER=""
for c in result/bin/s2c_enforce /nix/store/*s2c*/bin/s2c_enforce; do
	[ -x "$c" ] && { ENFORCER="$c"; break; }
done
[ -x "$ENFORCER" ] || { echo "FATAL: s2c_enforce not found"; exit 1; }

PASS=0; FAIL=0; TOTAL=0
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# config template
SCFG="$TMPDIR/tmpl/source-code-for-compiler-generation"
mkdir -p "$SCFG"
for f in allowed-names.txt banned-patterns.txt bootstrap-c-whitelist.txt \
         bootstrap-c-freeze-limits.txt allowed-non-source-files.txt; do
	[ -f "source-code-for-compiler-generation/$f" ] && cp "source-code-for-compiler-generation/$f" "$SCFG/"
done
for f in operator-signed-exemption-name-table.json operator-signed-exemption-name-table.sig \
         operator-signed-integrity-manifest-hashes.json operator-signed-integrity-manifest-hashes.sig; do
	[ -f "$f" ] && cp "$f" "$TMPDIR/tmpl/"
done

# setup workdir, run enforcer, check result
assert() {
	local name="$1" desc="$2" expect_fail="$3" workdir="$4"
	TOTAL=$((TOTAL+1))
	local log; log=$(mktemp)
	set +e
	"$ENFORCER" --lint "$workdir" >"$log" 2>&1
	local rc=$?
	set -e
	local ok=0
	if [ "$expect_fail" = "YES" ] && [ $rc -ne 0 ]; then ok=1; fi
	if [ "$expect_fail" = "NO" ]  && [ $rc -eq 0 ]; then ok=1; fi
	if [ $ok -eq 1 ]; then
		echo "[PASS] $name -- $desc"
		PASS=$((PASS+1))
	else
		echo "[FAIL] $name -- $desc  (rc=$rc want=$expect_fail)"
		sed 's/^/    /' "$log" | head -12
		FAIL=$((FAIL+1))
	fi
	rm -f "$log"
}

new_workdir() {
	local d; d=$(mktemp -d -p "$TMPDIR" "vt-XXXXXXXX")
	cp -a "$TMPDIR/tmpl/"* "$d/"
	mkdir -p "$d/scan-me"
	echo "$d"
}

# ── BYPASS 10: deep dir recursion (301 deep, limit is 256) ──
WD=$(new_workdir)
mkdir -p "$WD/scan-me/d"
dir="$WD/scan-me/d"
for i in $(seq 1 300); do dir="$dir/l$i"; mkdir "$dir"; done
cat > "$dir/test.c" <<'EOC'
int main(void){return 0;}
EOC
assert bypass10 "300-deep tree -> FATAL depth exceeded" YES "$WD"

# ── BYPASS 11: FIFO in scan dir -> skipped ──
WD=$(new_workdir)
cat > "$WD/scan-me/clean.c" <<'EOC'
int main(void){return 0;}
EOC
mkfifo "$WD/scan-me/test.c" 2>/dev/null || true
assert bypass11 "FIFO test.c skipped, clean.c passes" NO "$WD"

# ── BYPASS 12: symlink -> skipped (lstat) ──
WD=$(new_workdir)
cat > "$WD/scan-me/clean.c" <<'EOC'
int main(void){return 0;}
EOC
mkdir -p /tmp/s2c-bypass
cat > /tmp/s2c-bypass/bad.c <<'EOC'
int main(void){system("id");return 0;}
EOC
ln -s /tmp/s2c-bypass/bad.c "$WD/scan-me/link.c" 2>/dev/null || true
assert bypass12 "symlink to infected .c -> skipped by lstat" NO "$WD"

# ── BYPASS 13: dlopen() -> banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/dl.c" <<'EOC'
#include <dlfcn.h>
int main(void){
  void *h=dlopen("lib.so",RTLD_LAZY);
  return 0;
}
EOC
assert bypass13 'dlopen("lib.so") -> banned pattern' YES "$WD"

# ── BYPASS 14: system() -> banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/sys.c" <<'EOC'
#include <stdlib.h>
int main(void){system("id");return 0;}
EOC
assert bypass14 'system("id") -> banned pattern' YES "$WD"

# ── BYPASS 15: execvp() -> NOT banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/ex.c" <<'EOC'
#include <unistd.h>
int main(void){
  char*a[]={"/bin/sh","-c","id",NULL};
  execvp("sh",a);
  return 0;
}
EOC
assert bypass15 "execvp() -> not in banned patterns" NO "$WD"

# ── BYPASS 16: LD_PRELOAD string -> banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/ld.c" <<'EOC'
#include <stdlib.h>
int main(void){setenv("LD_PRELOAD","lib.so",1);return 0;}
EOC
assert bypass16 "LD_PRELOAD string -> banned pattern" YES "$WD"

# ── BYPASS 17: ptrace() -> banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/pt.c" <<'EOC'
#include <sys/ptrace.h>
int main(void){ptrace(PTRACE_TRACEME,0,0,0);return 0;}
EOC
assert bypass17 "ptrace(PTRACE_TRACEME) -> banned pattern" YES "$WD"

# ── BYPASS 18: __attribute__((constructor)) -> banned ──
WD=$(new_workdir)
cat > "$WD/scan-me/ct.c" <<'EOC'
#include <stdlib.h>
__attribute__((constructor)) void init(void){system("id");}
int main(void){return 0;}
EOC
assert bypass18 "__attribute__((constructor)) -> banned pattern" YES "$WD"

echo
echo "Results: $PASS/$TOTAL passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
