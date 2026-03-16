#!/usr/bin/env bash

set -u
set -o pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

if [[ ! -d "firmware" ]]; then
    echo "Error: run this script from the workspace root (missing ./firmware)."
    exit 2
fi

declare -a RESULTS=()
declare -a FAILURES=()
declare -a CLEAN_WARNINGS=()

run_step() {
    local name="$1"
    local workdir="$2"
    shift 2
    local -a cmd=("$@")

    echo "=================================================="
    echo "STEP: $name"
    echo "DIR : $workdir"
    echo "CMD : ${cmd[*]}"

    if (cd "$workdir" && "${cmd[@]}"); then
        RESULTS+=("PASS | $name")
        echo "RESULT: PASS"
    else
        local rc=$?
        RESULTS+=("FAIL | $name (exit $rc)")
        FAILURES+=("$name (exit $rc)")
        echo "RESULT: FAIL (exit $rc)"
    fi

    echo
}

run_clean_step() {
    local name="$1"
    local workdir="$2"
    shift 2
    local -a cmd=("$@")

    echo "--------------------------------------------------"
    echo "CLEAN: $name"
    echo "DIR  : $workdir"
    echo "CMD  : ${cmd[*]}"

    if (cd "$workdir" && "${cmd[@]}"); then
        echo "CLEAN RESULT: PASS"
    else
        local rc=$?
        CLEAN_WARNINGS+=("$name (exit $rc)")
        echo "CLEAN RESULT: FAIL (exit $rc) - continuing"
    fi

    echo
}

run_test_step() {
    local name="$1"
    local workdir="$2"

    # Test makefiles share object paths under firmware/common; clean right before
    # each suite to avoid cross-suite stale object contamination.
    run_clean_step "Pre-test clean: $name" "$workdir" make -j"$JOBS" clean
    run_step "$name" "$workdir" make -j"$JOBS" all
}

echo "Starting cleanup of previous build/test artifacts..."
run_clean_step "Clean dock firmware build" "firmware/dock" make -j"$JOBS" clean
run_clean_step "Clean ring firmware build" "firmware/ring" make -j"$JOBS" clean
run_clean_step "Clean dock unit test build" "firmware/dock/tests" make -j"$JOBS" clean
run_clean_step "Clean ring unit test build" "firmware/ring/tests" make -j"$JOBS" clean
run_clean_step "Clean common unit test build" "firmware/common/tests" make -j"$JOBS" clean
echo

run_step "Compile dock firmware" "firmware/dock" make -j"$JOBS" default
run_step "Compile ring firmware" "firmware/ring" make -j"$JOBS" default
run_test_step "Run dock unit tests" "firmware/dock/tests"
run_test_step "Run ring unit tests" "firmware/ring/tests"
run_test_step "Run common unit tests" "firmware/common/tests"

echo "========================== SUMMARY =========================="
for result in "${RESULTS[@]}"; do
    echo "$result"
done
if [[ ${#CLEAN_WARNINGS[@]} -gt 0 ]]; then
    echo "Cleanup warnings:"
    for warning in "${CLEAN_WARNINGS[@]}"; do
        echo " - $warning"
    done
fi
echo "============================================================="

if [[ ${#FAILURES[@]} -eq 0 ]]; then
    echo "SUCCESS: all compile/test checks passed."
    exit 0
fi

echo "FAILURE: ${#FAILURES[@]} check(s) failed:"
for failure in "${FAILURES[@]}"; do
    echo " - $failure"
done

exit 1
