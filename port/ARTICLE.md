---
title: "Digimon World's evolution algorithm was a mystery for 25 years. Someone decompiled it."
published: false
tags: reverseengineering, c, gamedev, retro
cover_image: ""
canonical_url: ""
---

You raised it for six hours. Fed it, trained it, cleaned up after it. You had a plan. And then it evolved into Numemon.

If you played Digimon World on the PlayStation in 1999, you know that specific feeling. The game never told you why. No menu, no hint, no stat screen that said *here is what went wrong*. Your Digimon just became a walking pile of sludge, and you started over.

For twenty-five years the community filled that silence with theories. Wikis listed thresholds. People built calculators. Forum threads argued about whether care mistakes helped or hurt. All of it reverse engineered from the outside, by playing the same section hundreds of times and writing down what happened.

It turns out the answer was sitting in the binary the whole time. And a small group of people has spent this year getting it out.

## The people who actually did this

[jype0/dw_decomp](https://github.com/jype0/dw_decomp) started in March 2026. As I write this it has 307 commits, 59 stars, and something genuinely rare: **zero `INCLUDE_ASM` stubs left in `src/`**.

If you have not worked on a decompilation project, that number needs explaining. `INCLUDE_ASM` is the marker for "we have not figured this function out yet, here is the raw assembly instead". Most decomps live with hundreds of them for years. This one has none. Every function in the executable has been rewritten as C that compiles back to the same machine code the original disc shipped with.

The work is not evenly spread, and credit should not be either:

| | |
|---|---|
| **Pekka Jylhä-Ollila (jype0)** | 232 commits. The maintainer, and the bulk of the project. |
| **juandav** | 37 merged PRs. Matching functions and data across the overlays. |
| **ChonkMode** | 11 merged PRs. |
| **kingzmanh**, **SydMontague** | matched functions, struct and naming reference work |
| **Frozen Burnside**, **ThirstyWraith**, **IngneieroGeomatico**, **paulohpfilho** | further contributions |

Ten people. Six months. A complete PS1 game.

I want to be precise about my own role here, because it is small: **I did not decompile anything.** I forked the repo out of curiosity, spent a day running their code in places it was never meant to run, and measured a few things. The hard part was already done when I arrived.

## Where the project is going

I opened an issue asking what the roadmap was. juandav answered with a screenshot of jype0 laying it out on Discord. Paraphrasing his four points:

1. **Cleanup.** Remove the hacks, use correct data structures, go file by file making the code readable and naming functions and variables properly.
2. **Match all 10 versions** of PS1 Digimon World. The US version being done should make the rest cheaper.
3. **Maybe a PC port.** His stated blocker: *psyq and psycross don't currently implement libgs or libsnd.*
4. **A DW1 mod of epic proportions.** New maps, story, Digimon. And this is the part I love: that was what he set out to do in the first place. The decomp was the detour.

Think about that for a second. Someone wanted to mod a 1999 game, discovered there was no clean way in, and decompiled the entire thing instead. The full reverse engineering of a PlayStation title, as a means to an end.

## The algorithm, finally readable

Here is what the game was doing all along, straight out of `src/main/evolution.c`.

Every Digimon has up to six possible evolution targets. For each candidate the game computes a score, and **a candidate needs at least 3 points to be eligible at all**. Below 3, it is not even considered.

### Point one: care mistakes, in both directions

```c
if (isMaxCM == 0) {
  if (partner->careMistakes >= reqs->care)
    reqPoints += 1;
} else if (partner->careMistakes <= reqs->care) {
  reqPoints += 1;
}
```

Read the comparisons. There is a flag bit that **inverts the test**. Some evolutions want your care mistakes *above* a number.

Neglect is not a punishment in this game. For certain paths it is a requirement. Every player who has accidentally raised a Numemon and blamed themselves was, mechanically speaking, meeting a requirement.

### Point two: weight, as a window

```c
if (reqs->weight - 5 <= partner->weight &&
    partner->weight <= reqs->weight + 5)
  reqPoints += 1;
```

Not a minimum. A band of **±5**. Overfeeding fails this check exactly as hard as underfeeding does. All those forum posts about keeping your Digimon at a specific weight were right, and the reason is four lines of C.

### Point three: stats, and it depends on the stage

```c
if (DIGIMON_DATA[target].level == 3U) {
  /* Champion: only your single HIGHEST stat is examined */
} else {
  /* everything else: ALL six stats must clear their thresholds */
}
```

For most evolutions you need every stat above its requirement. But for Champion-level targets, the game finds your highest stat and checks only that one. Specialising works for Champions. It does not work anywhere else.

### The bonus point: any one of five

```c
if (reqs->digimon != -1 && current == reqs->digimon)        isBonusFulfilled = 1;
if (reqs->discipline != -1 && ...)                          isBonusFulfilled = 1;
if (reqs->happiness != -1 && ...)                           isBonusFulfilled = 1;
if (reqs->battles != -1) { /* also invertible */ }
if (reqs->techs != -1)   { /* mastered moves */ }
```

Any single one of these grants the point. And battles carries the same inversion trick as care mistakes: some evolutions want you to have fought *fewer* than N battles.

## The detail nobody could have found by playing

This is my favourite thing in the whole file.

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

It is O(n²) across six elements, which is completely fine. But there is no `break`. The loop keeps running after it finds a maximum, so **when stats tie, the last index wins**.

The array order is `hp, mp, offense, defense, speed, brain`. I lifted the logic out verbatim and ran it:

```
all six tied      -> brain
offense highest   -> offense
off+def tied high -> defense
brain highest     -> brain
```

The tie-break priority is **brain > speed > defense > offense > mp > hp**.

Raise a perfectly balanced Digimon and the game silently treats it as a brain specialist. There is no way to learn that from the outside. It is not a stat you can see, not a message you get, not a pattern that survives noise. It falls out of a missing `break` in a loop written in 1999.

(I first thought `highestStat` might be read uninitialized, which would have been a real bug worth reporting upstream. It cannot be: a maximum always exists among six values. Worth checking before making the claim.)

## The rule that steers your collection

There is one more piece, and it made me sit back:

```c
if (reqPoints >= 3 && currentBest != -1) {
  isTargetRaised      = hasDigimonRaised(EVO_GAINS_DATA[target].targetDigimon);
  isCurrentBestRaised = hasDigimonRaised(EVO_GAINS_DATA[currentBest].targetDigimon);

  if (isTargetRaised == 1 && isCurrentBestRaised == 0)
    reqPoints = 0;      /* you already have this one: disqualify it */

  if (isTargetRaised == 0 && isCurrentBestRaised == 1)
    reqPoints++;        /* you don't have this one: push it ahead */
}
```

The game remembers every Digimon you have raised, and **actively steers you toward ones you have not**. Not with randomness. By zeroing a duplicate's score outright when a fresh option is on the table.

A 1999 game, on hardware with 2MB of RAM, quietly biasing your outcomes toward seeing more of itself. That is a design instinct I would have expected from a decade later.

## Running their code somewhere it has never run

My question was narrower: can this code run off the PlayStation at all?

`evolution.c` is 1,087 lines. Compiling it on x86-64 produced exactly **one** error:

```
libgte.h: No such file or directory
```

That is Sony's geometry coprocessor header. So I grepped `evolution.c` for `VECTOR`, `MATRIX`, `gte_`, `RotTrans`. Nothing. It does not use the GTE at all. It merely *inherits* the header through `entity.h`.

I wrote type-only stand-ins for the four PsyQ headers. It compiled clean, and then it ran:

```
EvoRequirements = 28 bytes (PS1 layout: 28)
fresh(1)       -> 2
in-training(3) -> 5
```

That 28 matters more than it looks. The game reads its data tables as raw structs. If the layout shifted by one byte on a 64-bit host, every evolution requirement in the game would decode as noise. It does not, and I pinned it with `static_assert` so nobody can break it quietly later.

## Measuring the hard part

The genuinely risky piece of any PS1 port is the GTE, the fixed-point geometry coprocessor.

My first instinct was to size it with a grep. I got 181 and quoted that number publicly. **It was wrong.** My pattern was also matching a header that *defines* every GTE macro whether the game uses it or not. Measured against `src/` alone:

```
632 call sites, 29 distinct operations, 34 of 126 files
```

And then the distribution rescued it:

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

**Eleven operations cover 80% of every GTE use in the game.** That is not 29 things to build. It is eleven.

I implemented those eleven in software. The contract is strict: matrix entries are 1.3.12 fixed point (4096 = 1.0), angles run 4096 to a full turn, and results **saturate rather than wrap**. That last one is not optional. A wrapped coordinate throws a vertex to the other side of the screen, and that is the kind of bug that ships without anyone noticing.

Then I measured the number that actually decides anything.

"0.17% trigonometric error" is a useless statistic. Nobody can tell you whether that is visible. So I measured pixels instead: how far a projected vertex lands from where double-precision maths would put it, across 2,528 vertices and a full rotation sweep.

```
mean error:             0.825 px
worst error:            1.896 px
vertices off by > 2px:  0.00%
```

Under two pixels everywhere. A player would never see it.

And here is the honest half: **it is not bit-exact**. The real GTE uses lookup tables. My polynomial approximation looks identical and is numerically different. Fine for a *port*. Useless for *verifying a decompilation*, because that project's entire premise is reproducing the original binary byte for byte.

Confusing those two would be the expensive mistake, so I put the warning in the test output rather than a comment.

## Drawing a frame

The game's rendering is all PsyQ primitives. Counted across the source:

```
POLY_FT4  386    (textured quad)
POLY_F4    30
POLY_GT4   17
POLY_FT3    6    (textured triangle)
```

Textured quads and triangles are about 392 of roughly 460 primitive uses, so that is the case worth proving. I built them with the game's own macros, `setXYWH` and `setUVWH` lifted straight from `src/main/utils.c`, and rasterised them in software.

One thing I deliberately did not fix: the texture mapping is affine, not perspective-correct. The PS1 had no perspective correction, and that wobble is part of how the console looks. Correcting it would make a port feel wrong.

Then the ordering table, which is how the PS1 sorted by depth. The name misleads, so it is worth saying plainly:

**An ordering table is not a sorting algorithm.** It is a bucket array. One slot per depth value, each holding a linked list, with `AddPrim` pushing to the front. O(1) insertion, zero comparisons. That is how a 33MHz console depth-sorted an entire scene every frame, and reimplementing it with `qsort` would be slower *and* wrong, because primitives at equal depth must keep their submission order.

I built the occlusion test so it could not pass by luck: submit the **near** quad first, then the far one.

```
normal:          centre pixel r=16  g=104    (near/green occludes)
depths swapped:  centre pixel r=120 g=16     (far/red now occludes)
```

Same submission order, opposite result. The table is doing the work, not the sequence of my function calls.

## Where I got things wrong

Two corrections, because they are more useful than a clean story.

**The GTE count.** I said 181 in public. It is 632. I estimated with a grep and then quoted the estimate as a measurement, which is a bad habit and I did it anyway.

**A test that failed for the right reason.** I asserted the far square would peek out around the near one. It does not: the near quad spans x=75..245, the far one x=123..197. Full occlusion is correct physics and my sampling point was simply badly chosen. I checked the rendered image before touching any code, which is the only reason I did not "fix" working code to match a broken expectation.

## What this does not prove

- **One logic file out of 126.** I picked `evolution.c` *because* it looked dependency-free. Best case, not a representative sample.
- **The GTE is approximate.** 18 of 29 operations unimplemented, and the trig is polynomial rather than table-driven.
- **No model loading.** `GsSortObject4` has 42 call sites and walks TMD models. The ordering table is ready for it; the format reader is not written.
- **Nothing validated against the retail binary.** That needs the MIPS toolchain and a disc image.
- **The assets are still Bandai's.** Models, textures, music, text. A port ships as an engine and each player supplies their own disc. That is the OpenRCT2 model, and it is the line between a project that lasts and one that gets a DMCA notice.

Worth noting where my work and jype0's roadmap touch: his stated blocker for a PC port is that *psyq and psycross don't implement libgs or libsnd*. `libgs` is one of the four headers I stubbed. I am not claiming that solves his problem, only that it is the same wall, and it is climbable from at least one side.

## What we finally understand

Twenty-five years of wiki edits, spreadsheets and arguments, and the answer is a few hundred lines of C:

- **3 points** to be eligible, out of a possible 4
- **Care mistakes and battle counts are invertible.** Some evolutions want neglect
- **Weight is a ±5 window.** Overfeeding fails like starving
- **Champions check your highest stat only.** Everything else checks all six
- **Ties favour brain**, because a loop from 1999 has no `break`
- **Duplicates get disqualified** when a Digimon you have never raised is available

Your Numemon was not bad luck. It was a score that never reached 3.

That is what these projects are for. Not nostalgia for its own sake, and definitely not piracy. **Making the rules legible again**, so a game people loved stops being a black box and becomes something you can read, verify, and eventually build on.

jype0 wanted to make a mod. To do it he had to make the whole game readable first. Ten people spent six months on that, and now anyone can open a file and see exactly why their Digimon turned into sludge in 1999.

---

The decomp is [jype0/dw_decomp](https://github.com/jype0/dw_decomp), MIT licensed. If any of this interests you, that is where the real work is, and the roadmap above is where it is heading.

My four experiments live on a [branch in my fork](https://github.com/krlz-dev/dw_decomp/tree/port-spike/port). Each runs with one command and needs only `gcc`, plus `libsdl2-dev` for the two that draw. No PlayStation toolchain, no disc image required.
