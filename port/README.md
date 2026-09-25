# Port spike — running decompiled game logic off the PlayStation

**Result: it works.** `src/main/evolution.c` compiles and executes on x86-64
**unmodified**, against portable stand-ins for Sony's PSY-Q headers.

```
$ ./build.sh

Data tables linked from the decomp:
  EVO_REQ_DATA    63 entries
  EVO_PATHS_DATA  62 entries
  EvoRequirements = 28 bytes (PS1: 28)

Calling getRookieEvolutionTarget() — the real function
  (target 0x1a wants weight 30, care <= 10)
Calling getFreshEvolutionTarget() and getInTrainingEvolutionTarget()
  fresh(1)       -> 2
  in-training(3) -> 5        <- a real evolution decision

PASSED — decompiled game logic executes unmodified off the PS1.
```

## What this proves

| | |
|---|---|
| Game logic compiles on x86 | yes, **zero changes** to `evolution.c` |
| Struct layout preserved | `EvoRequirements` = **28 bytes**, same as PS1 |
| Data tables readable | 63 requirement rows, 62 evolution paths |
| Real decisions execute | `getInTrainingEvolutionTarget(3)` → `5` |

The evolution system — the heart of Digimon World's breeding loop — has **no
real PlayStation dependency**. It only inherited Sony headers through
`entity.h`, never calling into them.

## What's here

```
port/include/libgte.h   SVECTOR, VECTOR, MATRIX, CVECTOR   (types only)
port/include/libgpu.h   RECT, DISPENV, DRAWENV
port/include/libgs.h    GsOT, GsDOBJ2, GsCOORDINATE2, TMD_STRUCT
port/include/libcd.h    CdlLOC, CdlFILE
port/harness.c          the 18 external symbols evolution.c needs
port/run_evolution.c    calls the real functions and checks the results
```

`static_assert`s pin the struct sizes. If a stub ever drifts from the PS1
layout, the build fails instead of silently misreading every data table.

## Friction found (worth knowing for the real port)

1. **32-bit assumptions.** `dw/types.h` has `typedef int intptr_t` — the decomp
   targets 32-bit MIPS. The stubs must use the game's own types, never
   `<stdint.h>`, or the definitions collide.

2. **`random()` collides with glibc.** `dw/math.h` declares `random(int32_t)`;
   glibc has `random(void)`. The harness declares the handful of libc functions
   it needs instead of including `<stdio.h>`.

3. **Zeroed globals produce `-1`, not a crash.** `calculateRequirementScore()`
   reads `PARTNER_PARA` and `DIGIMON_DATA`, which the game loads from disc.
   Empty tables make every candidate fail the weight check, so the function
   correctly reports "no evolution". That is right behaviour, not a port bug —
   it took feeding realistic values to see the decision path actually run.

## Honest limits

- **Only `evolution.c`.** One file of 126. It was picked because it had no PS1
  dependencies, so it is the best case, not a representative sample.
- **Stubs are types only.** No GTE maths, no rendering, no audio. The hard part
  — 181 fixed-point GTE uses and ~50 PSY-Q functions — is untouched.
- **Not validated against the original.** Without the MIPS toolchain and a disc
  image, there is no proof the decomp still matches the retail binary.
- `getRookieEvolutionTarget` still returns -1 here: it needs more of the
  game's loaded state than this harness fakes.

## Reproduce

```bash
cd port && ./build.sh
```

Needs only `gcc`. No PlayStation toolchain, no disc image.
