#!/bin/bash
# Build and run the full-pipeline spike: GTE -> ordering table -> rasteriser.
set -e
cd "$(dirname "$0")/.." || exit 1
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT

gcc -O1 -std=gnu11 -Wall -I port $(pkg-config --cflags sdl2) \
    -o "$OUT/run_scene" \
    port/gte.c port/ot.c port/gpu_sdl.c port/run_scene.c \
    $(pkg-config --libs sdl2) -lm

SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} "$OUT/run_scene"
