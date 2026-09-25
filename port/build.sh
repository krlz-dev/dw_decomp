#!/bin/bash
# Build and run the port spike: real decompiled game logic on x86.
# Requires only gcc — no PlayStation toolchain, no disc image.
set -e
cd "$(dirname "$0")/.." || exit 1

INC="-I port/include -I include -I ."
CFLAGS="-O1 -std=gnu11 -fno-stack-protector -Wall"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "=== compiling ==="
gcc -c $CFLAGS $INC -o "$OUT/evolution.o" src/main/evolution.c
echo "  evolution.c      (unmodified game code)"
gcc -c $CFLAGS $INC -o "$OUT/harness.o"   port/harness.c
echo "  harness.c"
gcc -c $CFLAGS $INC -o "$OUT/run.o"       port/run_evolution.c
echo "  run_evolution.c"

echo ""
echo "=== linking ==="
gcc -o "$OUT/run_evolution" "$OUT/evolution.o" "$OUT/harness.o" "$OUT/run.o"
echo "  ok"

echo ""
"$OUT/run_evolution"
