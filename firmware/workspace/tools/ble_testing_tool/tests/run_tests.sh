#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<'EOF'
Usage:
  ./run_tests.sh [TEST_TARGET]

Behavior:
  - No TEST_TARGET: run all test_*.py files in the current directory.
  - TEST_TARGET provided: run only that target.
    TEST_TARGET can be:
      - a test file (e.g. test_ad_ppi.py)
      - a pytest node id (e.g. test_ad_ppi.py::test_ppi_ad_time)

Examples:
  ./run_tests.sh
  ./run_tests.sh test_ad_ppi.py
  ./run_tests.sh test_ad_ppi.py::test_ppi_ad_time
EOF
  exit 0
fi

reports_dir="reports"
mkdir -p "${reports_dir}"

test_targets=()
if [[ $# -eq 0 ]]; then
  mapfile -t test_targets < <(find . -maxdepth 1 -type f -name 'test_*.py' -printf '%f\n' | sort)
  if [[ ${#test_targets[@]} -eq 0 ]]; then
    echo "No test_*.py files found in $(pwd)." >&2
    exit 1
  fi
  report_name="all_tests_report.html"
else
  test_target="$1"
  test_targets=("${test_target}")
  report_name="${test_target//\//_}"
  report_name="${report_name//::/_}"
  report_name="${report_name%.py}_report.html"
fi

MP_HIL_ENABLED=1 uv run pytest \
  --capture=fd \
  --show-capture=all \
  --log-level=INFO \
  -vv \
  -m hil \
  "${test_targets[@]}" \
  --html="${reports_dir}/${report_name}" \
  --self-contained-html
