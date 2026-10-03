# DW1 Research Notes for MONSTR

> **What this is:** measurements taken from the [jype0/dw_decomp](https://github.com/jype0/dw_decomp)
> decompilation, answering specific open questions in the MONSTR mechanics plan
> and region blueprint.
>
> **What this is not:** a source of code, assets, names, maps or story for
> MONSTR. Per §18 of the mechanics plan, this is research into *design
> principles*. Every number below is a measurement of how DW1 was built, not
> something to copy.

---

## Headline finding: the algorithm is small, the simulation is enormous

The MONSTR plan (§7) treats DW1's evolution logic as the thing worth studying.
Having now read it, that is the wrong place to look.

```
evolution.c        1,087 lines     <- the "mysterious" algorithm
partner.c + impl   3,256 lines     <- the simulation feeding it
script_interp.c
  + script_common  9,635 lines     <- the content engine
```

The evolution rule is roughly 150 lines of comparisons. What makes it *feel*
mysterious is the 65-field simulation underneath it, running continuously.

### `PartnerPara`: 65 fields, measured

This is the complete per-partner state DW1 persisted. 65 fields total: 53
named, 12 still unidentified (`unused1` through `unused12`).

**Sleep cycle (7 fields)** — `sleepyHour`, `sleepyMinute`, `wakeupHour`,
`wakeupMinute`, `hoursAwakeDefault`, `hoursAsleepDefault`, `timeAwakeToday`

**Health and sickness (9)** — `sicknessCounter`, `missedSleepHours`,
`virusBar`, `timesBeingSick`, `sicknessTries`, `sicknessTimer`, `injuryTimer`,
`areaEffectTimer`, `condition`

**Hunger and body (8)** — `nextHungerHour`, `energyLevel`, `foodLevel`,
`starvationTimer`, `weight`, `refusedFavFood`, `emptyStomachTimer`,
`tirednessHungerTimer`

**Waste (3)** — `poopLevel`, `poopingTimer`, plus the tick handler

**Tiredness (3)** — `tiredness`, `subTiredness`, `tirednessSleepTimer`

**Relationship (3)** — `discipline`, `happiness`, `careMistakes`

**Lifetime (3)** — `remainingLifetime`, `age`, `evoTimer`

**Training (9)** — `trainBoostFlag`, `trainBoostValue`, `trainBoostTimer`, plus
six per-stat upgrade counters

**Misc (2)** — `battles`, `fishCaught`

Plus six `sukaBackup*` fields (used when Sukamon steals your stats) and twelve
`unused*` slots nobody has identified yet.

### Why this matters for MONSTR

Your §7 lists evolution inputs: stats, training emphasis, combat behaviour,
care, relationship, environment, victories, decisions, special conditions.

That list is **good**, and DW1 proves it works. But the implication is that
MONSTR needs a continuously-running partner simulation, not just an evolution
function. The mystery players remember comes from *many slowly-changing
variables*, not from a clever formula.

**Scope warning:** 65 fields is a lot for a vertical slice. A defensible
minimum that still produces emergent-feeling outcomes:

```
hunger / food
tiredness / sleep
happiness
discipline
careMistakes
weight
battles
6 x per-stat training counters
```

Roughly 13 fields. Add the rest only when something needs them.

---

## There was no state machine. There were 256 integers.

This is the finding most relevant to your §13 and §15.

DW1's entire global world state lives in a flat array of **256 slots**, aliased
by name as they get identified:

```c
#define PSTAT_TIME_SPEED              PSTAT_0
#define PSTAT_PROSPERITY_POINTS       PSTAT_1
#define PSTAT_TOURNAMENT_DAY          PSTAT_2
#define PSTAT_TOURNAMENT_ID           PSTAT_3
#define PSTAT_TOURNAMENT_DIGIMON      PSTAT_4
#define PSTAT_SUKAMON_BACKUP_DIGIMON  PSTAT_5
```

Six of 256 identified after six months of work. The rest are still
`PSTAT_7`, `PSTAT_31`, `PSTAT_104`.

### What DW1 got right, and what it got wrong

**Right:** flat, indexable, trivially serialisable, cheap on 2MB of RAM. One
array to save, one array to load, no graph traversal.

**Wrong:** completely opaque. `PSTAT_104 = 3` tells you nothing. Six months of
reverse engineering has recovered six names. The *reason* DW1's progression is
hard to understand today is this array.

### MONSTR's §15 is already the fix

```
World.Coast.LighthouseActive
Settlement.Greenhouse.Level
Creature.Mossu.Recruited
Story.MainChapter
```

That is the same mechanism with names attached. Keep it, and resist any
optimisation that flattens it back into indices. The readability *is* the
feature — your §18 explicitly wants agents reasoning about this state, and no
agent will reason usefully about `PSTAT_104`.

**One borrowing worth making:** DW1's `PROSPERITY_POINTS` is a single integer
driving town growth. Your §3 growth stages (Abandoned → Camp → Village →
Connected → Living) could be exactly that: one derived value, with stages as
thresholds. Cheaper than tracking a dozen booleans and easier to tune.

---

## Content was data, not code: 280 opcodes

Your §12 (mission design) and §14 (data-driven world) propose objective
primitives. DW1 did this, at a scale worth knowing about.

`script_interp.c` is a **280-opcode virtual machine**. Named handlers include:

```
scriptCompareValues        scriptShowSelection
scriptCompareDate          scriptStartTournament
scriptSetDigimon           scriptCheckTournamentMedal
scriptLearnMove            scriptStartAnimation
scriptLoadModel            scriptUnloadEntity
```

Every conversation, cutscene, recruitment and world event in DW1 is **data
interpreted by this VM**, not C code. That is how a 1999 team shipped that much
content.

### For MONSTR

Your §12 primitive list (Talk, ReachLocation, Defeat, FindItem, GiveItem,
Follow, Escort, Interact, Discover, Repair, Recruit, WaitUntilTime,
RaiseRelationship, TriggerEvent, Choose) is **15 primitives**. DW1 needed 280.

The gap is informative but not alarming: much of DW1's 280 is low-level
(load model, set textbox size, start animation) that Unity gives you free.
Your 15 are the high-level ones.

But expect the list to grow. The honest planning number is probably 40-60
primitives by the time five regions are content-complete. Build the interpreter
so adding one is trivial, and your §13 `ICondition`/`IGameAction` interfaces are
already the right shape for that.

---

## World structure: DW1's overlays were systems, not regions

Relevant to your §17 (Unity scene strategy), where you are weighing
scene-per-area against region-scene-plus-chunks.

DW1 split its executable into overlays, which look like regions but are not:

```
main   btl    vs     std    trn    trn2   kar
fish   mov    evl    eab    doo2   dooa   murd
dget   endi
```

Those are **subsystems**: `btl` is battle, `vs` is versus mode, `fish` is
fishing, `trn`/`trn2` are training, `kar` is a minigame, `evl` is the evolution
sequence. The console had 2MB of RAM, so entire *features* were swapped in and
out, not places.

Maps themselves were data, loaded from `DOOR_MAPDATA` with per-map tile,
collision, lighting and warp tables.

### For MONSTR

The DW1 precedent does not favour either of your Option A or Option B, because
its constraint (2MB) no longer exists. But it does suggest a third framing:

**Areas as data, scenes as containers.** One scene per *region*, with areas
defined in your §16 region YAML and instantiated at runtime. That matches your
§18 agent workflow better than 30 hand-authored scene files, and it keeps the
world graph machine-readable, which your §16 says is the important part.

---

## Two mechanics worth stealing outright

### 1. Invertible requirements

DW1's evolution checks have a flag bit that reverses the comparison:

```c
if (isMaxCM == 0) {
  if (partner->careMistakes >= reqs->care) reqPoints += 1;
} else if (partner->careMistakes <= reqs->care) {
  reqPoints += 1;
}
```

Some evolutions require *high* care mistakes. Same trick on battle count: some
require having fought *fewer* than N.

This is extremely cheap (one bit) and creates genuine design space: a neglect
path and a devotion path share one data table. Worth having in MONSTR's
evolution data from day one.

### 2. Anti-duplicate steering

```c
if (isTargetRaised == 1 && isCurrentBestRaised == 0)
  reqPoints = 0;      /* already own it: disqualify */
if (isTargetRaised == 0 && isCurrentBestRaised == 1)
  reqPoints++;        /* don't own it: nudge ahead */
```

DW1 tracked every Digimon you had raised and actively biased outcomes toward
ones you had not. Not randomness — outright disqualification of duplicates when
a fresh option scored high enough.

For MONSTR this maps directly onto §11 (main vs optional progression): the same
mechanism could steer players toward unrecruited monsters without ever telling
them it is happening.

---

## Scope reality check

Your §3 targets ~25 recruitable monsters, 5 regions, 15-25 hours. For calibration
against what DW1 actually cost:

```
308 commits · 6 months · 10 contributors · ~113k lines of logic
```

That is the cost of *understanding* an already-finished game, with no art, no
design and no iteration. It is not a comparison to building MONSTR, but it is a
useful reminder of how much system a game of this shape contains underneath the
content.

Your §19 vertical slice (town + forest + Mossu + one evolution + save/load) is
the correct response to that. It is small enough to finish and complete enough
to prove the loop.

---

## What I have not researched

Being explicit about gaps, since these could change recommendations:

- **How recruitment actually triggered.** I found `PROSPERITY_POINTS` but not
  the code path that increments it on a recruit.
- **The map/area count.** `DOOR_MAPDATA` exists but I did not count its entries,
  so I cannot tell you how many screens DW1 shipped.
- **Battle system structure.** `btl` is 14,962 lines across 7 files and I have
  not opened it. Relevant to your §8.
- **The evolution tree itself.** I read the *algorithm*, not the 63-row
  requirement table. Deliberately: your §7 says to build MONSTR's own model, and
  reading their tree in detail risks anchoring on it.

---

## Legal position, unchanged

Per §20 rule 12 of the mechanics plan. Everything above is a measurement of
*architecture*: field counts, opcode counts, state-array sizes, two mechanical
patterns. No DW1 code, data tables, monster names, map layouts, dialogue or
assets belong in MONSTR.

The two mechanics flagged as "worth stealing" are **patterns**, not
implementations: an invertible-comparison bit and a duplicate-penalty rule. Both
are the kind of idea that appears in design literature, and MONSTR should
implement them from scratch against its own data model.
