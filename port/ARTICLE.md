---
title: "I decompiled Digimon World's evolution algorithm and ran it on x86"
published: false
tags: reverseengineering, c, gamedev, cpp
cover_image: ""
canonical_url: ""
---

Digimon World came out in 1999. For twenty-five years players have traded theories about how evolution actually works in that game, because it never tells you. You raise a Digimon for hours, it evolves into something, and you have no idea why.

The community reverse engineered a lot of it through experimentation. Wikis list stat thresholds. People built calculators. But experimentation gives you correlations, not the rule.

There's now [a decompilation project](https://github.com/jype0/dw_decomp) that has the actual rule. Around 175,000 lines of C (roughly 113,000 once you exclude the generated data tables), and zero assembly stubs left in `src/`. I spent a day poking at it to answer a different question, and ended up reading the algorithm instead.

This is what's in there, plus what I measured about running it off the console.

## The algorithm

Here's the shape of it. Each Digimon has up to six possible evolution targets. For each candidate, the game computes a score, and **a candidate needs at least 3 points to be eligible at all**.

Three points, from these sources:

**Point 1: care mistakes.** Either you're under the threshold, or over it. And this is the part I didn't expect:

```c
if (isMaxCM == 0) {
  if (partner->careMistakes >= reqs->care)
    reqPoints += 1;
} else if (partner->careMistakes <= reqs->care) {
  reqPoints += 1;
}
```

Look at the comparison direction. Some evolutions want *many* care mistakes. There's a flag bit that inverts the test. Neglect isn't a penalty, it's a requirement for certain paths.

**Point 2: weight, within a window.**

```c
if (reqs->weight - 5 <= partner->weight &&
    partner->weight <= reqs->weight + 5)
  reqPoints += 1;
```

Not a minimum. A band of ±5. Overfeeding fails the same check as underfeeding, which explains a lot of frustrated forum posts.

**Point 3: stats, and this is where it gets interesting.** The check depends on the target's evolution stage:

```c
if (DIGIMON_DATA[target].level == 3U) {
  /* Champion: only your HIGHEST stat is examined */
} else {
  /* everything else: ALL six stats must clear their thresholds */
}
```

For most evolutions you need every stat above its requirement. But for Champion-level targets, the game finds your single highest stat and checks only that one.

**Bonus point:** any *one* of these is enough, not all of them.

```c
if (reqs->digimon != -1 && current == reqs->digimon)     isBonusFulfilled = 1;
if (reqs->discipline != -1 && ...)                       isBonusFulfilled = 1;
if (reqs->happiness != -1 && ...)                        isBonusFulfilled = 1;
if (reqs->battles != -1) { ... }                          /* also invertible */
if (reqs->techs != -1) { ... }
```

Battles has the same inversion trick as care mistakes. Some evolutions want you to have fought *fewer* than N battles.

## The detail you'd never find by playing

That "highest stat" loop is written like this:

```c
for (i = 0; i < 6; i++) {
  int8_t isHighestStat = 1;
  for (j = 0; j < 6; j++) {
    if (statsArray[i] < statsArray[j])
      isHighestStat = 0;
  }
  if (isHighestStat == 1)
    highestStat = i;
}
```

It's O(n²) over six elements, which is fine. But notice there's no `break`. It keeps going after finding a maximum, so **on a tie, the last index wins**.

The array order is `hp, mp, offense, defense, speed, brain`. I pulled the logic out verbatim and ran it against a few stat sets:

```
all six tied      -> brain
offense highest   -> offense
off+def tied high -> defense
brain highest     -> brain
```

So the tie-break priority is **brain > speed > defense > offense > mp > hp**. If you raise a perfectly balanced Digimon, the game treats it as a brain specialist. Nothing in the game communicates that, and I don't think you could infer it from play.

(I briefly thought `highestStat` could be read uninitialized here, which would have been a real bug worth reporting. It can't: a maximum always exists among six values, so the loop always assigns. Worth checking before claiming it.)

## The anti-farming rule

There's one more piece that I found genuinely clever:

```c
if (reqPoints >= 3 && currentBest != -1) {
  isTargetRaised      = hasDigimonRaised(EVO_GAINS_DATA[target].targetDigimon);
  isCurrentBestRaised = hasDigimonRaised(EVO_GAINS_DATA[currentBest].targetDigimon);

  if (isTargetRaised == 1 && isCurrentBestRaised == 0)
    reqPoints = 0;      /* already have it: disqualify entirely */

  if (isTargetRaised == 0 && isCurrentBestRaised == 1)
    reqPoints++;        /* don't have it: nudge it ahead */
}
```

The game tracks which Digimon you've already raised, and actively steers you toward ones you haven't. Not by randomness. By zeroing the score of a duplicate when a fresh option exists.

A 1999 game quietly biasing your outcomes toward collection completeness. That's a design decision I'd have expected from a much later era.

## Getting it to run off the PlayStation

The reason I opened the repo wasn't the algorithm, it was a portability question. So I checked: does this code have any actual PS1 dependency?

`evolution.c` is 1,087 lines. Compiling it on x86-64 gave exactly **one** error:

```
libgte.h: No such file or directory
```

`libgte.h` is Sony's geometry coprocessor header. I grepped `evolution.c` for `VECTOR`, `MATRIX`, `gte_`, `RotTrans`. Zero hits. It doesn't use the GTE at all, it just *inherits* the header through `entity.h`.

I wrote type-only stand-ins for the four PsyQ headers, and it compiled clean. Then it ran:

```
EvoRequirements = 28 bytes (PS1 layout: 28)
fresh(1)       -> 2
in-training(3) -> 5
```

That 28 bytes matters more than it looks. The game's data tables get read as raw structs, so if the layout shifted by a single byte on a 64-bit host, every evolution requirement in the game would decode as garbage. It doesn't. I pinned it with `static_assert` so a future change can't silently break it.

## Measuring the hard part

The genuinely risky piece of any PS1 port is the GTE, the fixed-point geometry coprocessor. My first instinct was to estimate its size by grepping. I got 181 uses and quoted that number.

It was wrong. The grep was also matching the macro header that *defines* every GTE operation whether the game uses it or not. Measured properly against `src/` only:

```
632 call sites, 29 distinct operations, 34 of 126 files
```

And the distribution is what made this tractable:

| operation | sites | cumulative |
|---|---|---|
| `ApplyMatrixSV` | 103 | 16% |
| `RotMatrix` | 51 | 24% |
| `RotMatrixZYX` | 44 | 31% |
| `RotMatrixYXZ` | 44 | 38% |
| `ratan2` | 43 | 45% |
| `gte_rtps` | 42 | 58% |
| `ScaleMatrix` | 39 | 71% |
| `TransMatrix` | 37 | 77% |
| `ApplyMatrixLV` | 23 | **81%** |

**Eleven operations cover 80% of all uses.** That's not 29 things to build, it's eleven.

I implemented those eleven in software. The contract is strict: matrix entries are 1.3.12 fixed point (4096 = 1.0), angles run 4096 to a full turn, and results **saturate rather than wrap**. That last one isn't optional. A wrapped coordinate teleports a vertex across the screen, which is the kind of bug that ships unnoticed.

Then came the number that actually decides feasibility.

My first result was "0.17% trig error". That number is useless. Nobody can tell you whether 0.17% is visible. So I measured the thing that matters instead: how many **pixels** a projected vertex lands away from where double-precision maths would put it. 2,528 vertices, full rotation sweep.

```
mean error:             0.825 px
worst error:            1.896 px
vertices off by > 2px:  0.00%
```

Under two pixels everywhere. Invisible in play.

But here's the honest part, and it matters: **that's not bit-exact**. The real GTE uses lookup tables. My polynomial approximation looks identical and is numerically different. Good enough to *port* the game, useless to *verify a decompilation*, because that project's entire premise is matching the original binary byte for byte.

Conflating those two would be the expensive mistake. I put it in the test output so nobody reads it the wrong way.

## Drawing it

The game's rendering is all PsyQ primitives. Counted:

```
POLY_FT4  386    (textured quad)
POLY_F4    30
POLY_GT4   17
POLY_FT3    6    (textured triangle)
```

Textured quads and triangles are ~392 of ~460 total, so that's the case worth proving. I built them using the game's own macros, `setXYWH` and `setUVWH`, straight out of `src/main/utils.c`, and rasterised them in software.

One thing I deliberately did *not* fix: the texture mapping is affine, not perspective-correct. The PS1 had no perspective correction, and that warping is part of how the console looks. "Correcting" it would make the port look wrong.

Then the ordering table, which is how the PS1 sorted by depth. It's worth saying what it actually is, because the name misleads:

**An ordering table is not a sorting algorithm.** It's a bucket array. One slot per depth value, each holding a linked list, and `AddPrim` pushes to the front. O(1) insertion, zero comparisons. That's how a 33MHz console depth-sorted a full scene every frame.

Reimplementing it with `qsort` would be slower *and* wrong, because primitives at equal depth have to keep their submission order.

I built the occlusion test so it couldn't pass by accident. Submit the **near** quad first, then the far one. If the table works, near still wins:

```
normal:          centre pixel r=16  g=104    (near/green occludes)
depths swapped:  centre pixel r=120 g=16     (far/red now occludes)
```

Same submission order, inverted result. The table is doing the work, not the sequence of my function calls.

## Where I was wrong

Two corrections worth naming, because they're the useful part.

**The GTE count.** I said 181, it's 632. My grep pattern matched a header of macro definitions. Estimating by grep and then quoting the number as a measurement is a bad habit and I did it.

**A test I wrote that failed for the right reason.** I asserted the far square would peek out around the near one. It doesn't: the near quad spans x=75..245, the far one x=123..197. Full occlusion is correct physics and my sampling point was just badly chosen. I checked the rendered image before touching any code, which is the only reason I didn't "fix" working code to match a broken expectation.

## What this doesn't prove

Being straight about scope:

- **One logic file out of 126.** I picked `evolution.c` *because* it looked dependency-free. Best case, not a representative sample.
- **The GTE is approximate.** 18 of 29 operations unimplemented, trig is polynomial rather than table-driven.
- **No model loading.** `GsSortObject4` has 42 call sites and walks TMD models. The ordering table is ready for it, the format reader isn't written.
- **Nothing validated against the retail binary.** That needs the MIPS toolchain and a disc image.
- **Assets are still Bandai's.** Models, textures, music, text. A port ships as an engine and each user supplies their own disc. That's the OpenRCT2 model, and it's the line between a project that survives and one that gets a DMCA notice.

## Why the algorithm matters more than the port

The port might never happen. Someone has to write the TMD loader, the remaining GTE tail, audio, input, and the asset pipeline. That's months.

But the algorithm is already out. Twenty-five years of wiki speculation about Digimon World evolution, and the answer was sitting in a binary the whole time: a 3-point threshold, a ±5 weight window, invertible care and battle checks, highest-stat-only for Champions, a tie-break that silently favours brain, and an anti-duplicate rule that steers you toward Digimon you haven't raised.

That's what decompilation projects are actually for. Not nostalgia. Not piracy. **Making the rules legible.**

---

The decomp is [jype0/dw_decomp](https://github.com/jype0/dw_decomp), MIT licensed, and the credit for the hard part belongs entirely to the people who spent six months getting it to zero assembly stubs.

My four experiments are on a [branch in my fork](https://github.com/krlz-dev/dw_decomp/tree/port-spike/port). Each one runs with a single command and needs only `gcc`, plus `libsdl2-dev` for the two that draw. No PlayStation toolchain, no disc image.
