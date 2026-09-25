#!/bin/bash
# Build and run the renderer spike: PS1 primitives through SDL2.
#
# Note the split: gpu_sdl.c sees SDL and system headers; run_triangle.c and
# prim_glue.c see the game's 32-bit MIPS types. They never meet in one
# translation unit — that fence IS the port's architecture.
set -e
cd "$(dirname "$0")/.." || exit 1

GAME_INC="-I port/include -I include -I . -I port"
CFLAGS="-O1 -std=gnu11 -fno-stack-protector -Wall"
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT

echo "=== compiling ==="
gcc -c $CFLAGS -I port $(pkg-config --cflags sdl2) -o "$OUT/gpu.o" port/gpu_sdl.c
echo "  gpu_sdl.c       (SDL side)"
gcc -c $CFLAGS $GAME_INC -o "$OUT/glue.o" port/prim_glue.c
echo "  prim_glue.c     (the boundary)"
gcc -c $CFLAGS $GAME_INC -o "$OUT/run.o" port/run_triangle.c
echo "  run_triangle.c  (game side)"

gcc -o "$OUT/run_triangle" "$OUT/gpu.o" "$OUT/glue.o" "$OUT/run.o" $(pkg-config --libs sdl2)
echo "  linked"
echo ""
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} "$OUT/run_triangle"
