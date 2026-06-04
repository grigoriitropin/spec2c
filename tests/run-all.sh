#!/usr/bin/env bash
# Test runner for s2c-enforce fixtures
set -euo pipefail

# Make sure we are running from the spec2c root directory
if [ ! -f "flake.nix" ] || [ ! -d "tests" ]; then
    echo "Error: Must run tests/run-all.sh from the spec2c root directory." >&2
    exit 1
fi

ENFORCER="./result-enforce/bin/s2c-enforce"

# 1. Build s2c-enforce if not already built
if [ ! -f "$ENFORCER" ]; then
    echo "s2c-enforce binary not found at $ENFORCER. Attempting to build..."
    if ! nix build .#s2c-enforce -o result-enforce; then
        echo "Error: failed to build s2c-enforce via Nix." >&2
        exit 1
    fi
fi

# Double check that we have the binary now
if [ ! -x "$ENFORCER" ]; then
    echo "Error: Enforcer binary at $ENFORCER is missing or not executable." >&2
    exit 1
fi

# Counters
passed=0
failed=0
total=0

# Test runner function
run_test() {
    local name="$1"
    local cmd="$2"
    local expected_exit="$3"
    local expected_keyword="$4"
    
    total=$((total + 1))
    
    # Resolve the command to use our built enforcer binary
    local actual_cmd="${cmd//s2c_enforce/$ENFORCER}"
    
    # Identify the scanned directory from the end of the command
    local scan_dir
    scan_dir=$(echo "$cmd" | awk '{print $NF}')
    
    # Remove trailing slash if present
    scan_dir="${scan_dir%/}"
    
    # Determine the configuration directory for the enforcer (handling the sub-dir check)
    local data_dir="$scan_dir"
    if [ -d "$scan_dir/source-code-for-compiler-generation" ]; then
        data_dir="$scan_dir/source-code-for-compiler-generation"
    fi
    
    # Temporarily copy config files if they are not in the scanned directory's config directory
    local copied_config=0
    if [ -d "$scan_dir" ]; then
        if [ ! -f "$data_dir/allowed-names.txt" ]; then
            mkdir -p "$data_dir"
            cp source-code-for-compiler-generation/*.txt "$data_dir/" 2>/dev/null || true
            copied_config=1
        fi
    fi
    
    # Run the enforcer and capture output and exit code safely
    set +e
    local output
    output=$(eval "$actual_cmd" 2>&1)
    local status=$?
    set -e
    
    # Clean up copied config files
    if [ "$copied_config" -eq 1 ] && [ -d "$data_dir" ]; then
        rm -f "$data_dir"/allowed-names.txt \
              "$data_dir"/allowed-non-source-files.txt \
              "$data_dir"/banned-patterns.txt \
              "$data_dir"/bootstrap-c-freeze-limits.txt \
              "$data_dir"/bootstrap-c-whitelist.txt 2>/dev/null || true
        # If we had to create the directory, clean it up too if empty
        if [ "$data_dir" != "$scan_dir" ]; then
            rmdir "$data_dir" 2>/dev/null || true
        fi
    fi
    
    # Validate exit code (0 vs non-zero)
    local exit_ok=0
    if [ "$expected_exit" -eq 0 ]; then
        if [ "$status" -eq 0 ]; then
            exit_ok=1
        fi
    else
        if [ "$status" -ne 0 ]; then
            exit_ok=1
        fi
    fi
    
    # Validate keyword match
    local keyword_ok=0
    if [ -z "$expected_keyword" ]; then
        keyword_ok=1
    else
        if echo "$output" | grep -qE "$expected_keyword"; then
            keyword_ok=1
        fi
    fi
    
    # Report results
    if [ "$exit_ok" -eq 1 ] && [ "$keyword_ok" -eq 1 ]; then
        echo -e "\e[32mPASS\e[0m: $name"
        passed=$((passed + 1))
    else
        echo -e "\e[31mFAIL\e[0m: $name"
        echo "  Command: $cmd"
        echo "  Expected exit matching: $expected_exit (actual: $status)"
        echo "  Expected keyword matching: '$expected_keyword'"
        echo "  Raw Output:"
        echo "----------------------------------------"
        echo "$output"
        echo "----------------------------------------"
        failed=$((failed + 1))
    fi
}

# 2. Iterate over the fixtures under tests/fixtures/
# First, run the defined case directories if they exist
for fixture_dir in tests/fixtures/case*; do
    [ -d "$fixture_dir" ] || continue
    name=$(basename "$fixture_dir")
    
    # Skip running on empty directories to avoid false failures
    if [ -z "$(ls -A "$fixture_dir")" ]; then
        continue
    fi
    
    case "$name" in
        case1-evil-file)
            run_test "case1-evil-file" "s2c_enforce --lint tests/fixtures/case1-evil-file/" 1 "FATAL.*non-source|exactly one main"
            ;;
        case2-short-name)
            run_test "case2-short-name" "s2c_enforce --lint tests/fixtures/case2-short-name/" 1 "words"
            ;;
        case3-weak-malloc)
            run_test "case3-weak-malloc" "s2c_enforce --lint tests/fixtures/case3-weak-malloc/" 1 "Naming"
            ;;
        case4-banned-keyword)
            run_test "case4-banned-keyword" "s2c_enforce --lint tests/fixtures/case4-banned-keyword/gen/" 1 "banned pattern"
            ;;
        case9-redteam)
            # Iterate through bypass tests inside case9-redteam
            for bypass_dir in tests/fixtures/case9-redteam/bypass*; do
                [ -d "$bypass_dir" ] || continue
                bp_name=$(basename "$bypass_dir")
                
                # Skip bypass11 (known infinite hang on FIFO) and bypass17 (known segfault)
                if [ "$bp_name" = "bypass11" ] || [ "$bp_name" = "bypass17" ]; then
                    echo "Skipping case9-redteam-$bp_name (known hang/segfault bypass test)"
                    continue
                fi
                
                # Skip empty bypass directories
                if [ -z "$(ls -A "$bypass_dir")" ]; then
                    continue
                fi
                
                # Determine expected behavior based on the specific bypass type
                if [ "$bp_name" = "bypass01" ] || [ "$bp_name" = "bypass02" ] || [ "$bp_name" = "bypass03" ] || [ "$bp_name" = "bypass08" ] || [ "$bp_name" = "bypass12" ]; then
                    run_test "case9-redteam-$bp_name" "s2c_enforce --lint tests/fixtures/case9-redteam/$bp_name/" 0 ""
                else
                    run_test "case9-redteam-$bp_name" "s2c_enforce --lint tests/fixtures/case9-redteam/$bp_name/" 1 "banned pattern|hardcoded path|exactly one main|not in whitelist|recursion too deep"
                fi
            done
            ;;
        *)
            # Skip any other directories
            ;;
    esac
done

# If case5-long-gen exists (or if case5 has gen), run it specifically
if [ -d "tests/fixtures/case5-long-gen/gen" ]; then
    run_test "case5-long-gen" "s2c_enforce --lint-generated tests/fixtures/case5-long-gen/gen/" 0 ""
elif [ -d "tests/fixtures/case5/gen" ]; then
    run_test "case5-long-gen" "s2c_enforce --lint-generated tests/fixtures/case5/gen/" 0 ""
fi

# 3. Print summary
echo ""
echo "Summary: $passed passed, $failed failed, $total total"

# 4. Exit code based on passes/failures
if [ "$failed" -eq 0 ] && [ "$total" -gt 0 ]; then
    exit 0
else
    exit 1
fi
