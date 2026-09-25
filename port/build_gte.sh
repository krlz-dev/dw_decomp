#!/bin/bash
# Build and run the GTE measurements.
set -e
cd "$(dirname "$0")/.." || exit 1
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT

echo "=== contract tests ==="
gcc -O1 -std=gnu11 -Wall -I port -o "$OUT/run_gte" port/gte.c port/run_gte.c -lm
"$OUT/run_gte"

echo "=== accuracy in screen pixels ==="
gcc -O1 -std=gnu11 -Wall -I port -o "$OUT/run_err" port/gte.c port/run_gte_error.c -lm
"$OUT/run_err"
