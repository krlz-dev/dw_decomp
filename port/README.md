# Port spike — is a modern port of Digimon World realistic?

Two experiments, both passing. Together they cover the two things that decide
whether a port is worth starting: **does the game logic run off the PS1**, and
**can its rendering primitives be drawn by something that isn't Sony's GPU**.

```bash
./port/build.sh           # 1. game logic on x86
./port/build_triangle.sh  # 2. PS1 primitives through SDL2
```

Neither needs a PlayStation toolchain or a disc image. The second needs SDL2.

---

## 1. Game logic runs unmodified

`src/main/evolution.c` compiles and executes on x86-64 with **zero changes**,
against portable stand-ins for Sony's PSY-Q headers.

```
Data tables linked from the decomp:
  EVO_REQ_DATA    63 entries
  EVO_PATHS_DATA  62 entries
  EvoRequirements = 28 bytes (PS1: 28)

  fresh(1)       -> 2
  in-training(3) -> 5        <- a real evolution decision

PASSED — decompiled game logic executes unmodified off the PS1.
```

The evolution system turned out to have **no real PlayStation dependency**: it
only inherited `libgte.h` through `entity.h` and never called into it.

## 2. PS1 primitives rasterise through SDL2

![result](triangle-result.png)

Built with the **game's own macros** — `setXYWH` / `setUVWH`, exactly what
`src/main/utils.c` calls — then rasterised by `port/gpu_sdl.c`.

```
  POLY_FT4 = 40 bytes    POLY_FT3 = 32 bytes    POLY_F4 = 24 bytes

Textured triangle:   4141 pixels
Textured quad:      10201 pixels
  ok  V orientation correct (top stripe is white)
  ok  a tinted quad is darker than an untinted one

PASSED — PS1 primitives rasterise correctly through SDL2.
```

Why these primitives: a survey of the decomp shows what the game actually emits.

| primitive | uses |
|---|---|
| `POLY_FT4` | **386** |
| `POLY_F4` | 30 |
| `POLY_GT4` | 17 |
| `POLY_FT3` | 6 |

Textured tris and quads are ~392 of ~460 primitive uses, so this is the
representative case rather than a toy.

---

## The architecture this forced

The most useful thing to come out of the spike is a boundary, not code.

```
gpu_sdl.c     sees SDL + system headers        (plain C types only)
prim_glue.c   THE FENCE — converts between them
run_*.c       sees the game's 32-bit MIPS types
```

They must never meet in one translation unit. `dw/types.h` has
`typedef int intptr_t` and its own `int8_t`; SDL drags in `<stdint.h>`.
Compiling both together is a hard conflict, and the first attempt here hit it
head-on. The fix is not a macro trick — it is keeping the renderer ignorant of
the game's type universe, which is the right shape for a port anyway.

## Friction worth recording

1. **32-bit assumptions.** The decomp targets MIPS. Stubs must use the game's
   own types, never `<stdint.h>`.
2. **`random()` collides with glibc.** `dw/math.h` declares `random(int32_t)`;
   glibc has `random(void)`.
3. **Zeroed globals produce `-1`, not a crash.** `calculateRequirementScore()`
   reads `PARTNER_PARA` and `DIGIMON_DATA`, loaded from disc in the real game.
   Empty tables fail the weight check, so "no evolution" is *correct*. It took
   feeding realistic values to see the decision path actually run — mistaking
   that for a bug would cost weeks.
4. **Affine texture mapping is deliberate.** The PS1 had no perspective
   correction; its texture warping is part of the look. "Fixing" it would make
   the port look wrong.

## What's here

```
port/include/libgte.h   SVECTOR, VECTOR, MATRIX, CVECTOR
port/include/libgpu.h   RECT, POLY_FT3/FT4/F4/GT4, LINE_F2 + the PSY-Q macros
port/include/libgs.h    GsOT, GsDOBJ2, GsCOORDINATE2, TMD_STRUCT
port/include/libcd.h    CdlLOC, CdlFILE
port/harness.c          the 18 externals evolution.c needs
port/gpu_sdl.c          software rasteriser: textured tris/quads, CLUT, tinting
port/prim_glue.c        the type fence
port/run_evolution.c    calls the real game functions
port/run_triangle.c     builds primitives with the game's macros
```

`static_assert`s pin every struct size. If a stub drifts from the PS1 layout
the build fails, instead of silently misreading every data table.

## Honest limits

- **One logic file out of 126**, chosen because it was the best case.
- **No GTE.** The 181 fixed-point coprocessor uses are untouched, and that is
  the real technical risk: get the arithmetic wrong and the game looks *almost*
  right in ways that are miserable to debug.
- **No ordering table.** Primitives are drawn immediately; the PS1 sorts them
  by depth through `GsSortObject`/`GsDrawOt`.
- **No semi-transparency, no Gouraud, no CLUT4/8 indexed textures.**
- **Nothing validated against the retail binary** — that needs the MIPS
  toolchain and a disc image.
- **Assets remain Bandai's.** A port ships as an engine; each user supplies
  their own disc.
