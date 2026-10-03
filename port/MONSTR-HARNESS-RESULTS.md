# MONSTR — Headless Harness Results

> **For:** the MONSTR team
> **Question answered:** can the vertical slice be built and verified without
> Unity, a renderer, or a GPU?
> **Answer:** yes. 36 tests, 95 ms, zero Unity dependency.
>
> Everything below was executed, not estimated. The commands to reproduce it
> are at the bottom.

---

## 1. Headline result

```
Passed!  Failed: 0, Passed: 36, Skipped: 0, Total: 36, Duration: 95 ms
```

**Every item on the region blueprint §19 checklist is covered**, including the
final one — *"quit/reload, state remains correct"* — with no Unity process
involved.

| | |
|---|---|
| Core library | 902 lines C# |
| Test suite | 474 lines, 36 tests |
| Run time | 95 ms |
| Unity required | no |
| GPU required | no |
| Warnings | zero, with `TreatWarningsAsErrors` |

That is the measurement that decides the infrastructure question. The agent and
CI work in blueprint §18 is ~80% state, data and graph logic, and all of it runs
on a CPU-only box in under a second.

---

## 2. The guards were proven in RED

A test that passes whether or not the code is correct is worse than no test. So
each critical protection was deliberately broken to confirm the suite catches
it:

| Removed protection | Tests that went red |
|---|---|
| Atomic rollback in `GuardedEvent` | **1** |
| Unregistered state keys throwing | **1** |
| Defeat and recruit as separate steps | **3** |
| The gate on the Overgrown Ruins exit | **2** |

```
baseline                              Failed: 0, Passed: 36
remove atomic rollback                Failed: 1, Passed: 35
allow unregistered keys silently      Failed: 1, Passed: 35
collapse defeat into recruit          Failed: 3, Passed: 33
make the ruins exit ungated           Failed: 2, Passed: 34
restored                              Failed: 0, Passed: 36
```

The third row is the important one. **Collapsing "defeated" into "recruited"
breaks three tests**, which means the roster document's core rule — *winning a
fight does not mean the monster joins* — is now enforced by the build rather
than by memory. If someone simplifies that away in six months, CI stops them.

---

## 3. What the harness models

### Named state, not opaque integers

Direct response to what the DW1 measurement found: that game kept its entire
world state in a flat array of 256 integers, of which **6 have been identified
after six months** of reverse engineering by ten people. `PSTAT_104 = 3` tells
you nothing, and that opacity is precisely why DW1's progression is hard to
reason about today.

MONSTR uses the blueprint §15 naming instead:

```
Creature.Mossu.Recruited
World.Forest.VegetationDamageFound
Settlement.Greenhouse.Level
Story.MainChapter
```

Named keys fix readability but introduce a new failure mode: a typo silently
reads as `false`, and the bug surfaces hours later as *"the door never opened"*.
So **every key is registered, and touching an unregistered key throws** with a
suggestion:

```
Unregistered state key 'Creature.Mosu.Recruited'.
Did you mean: Creature.Mossu.Recruited?
```

Loud failure beats a silent default, especially when an agent is writing the
data files.

### Events are gated, atomic and idempotent

A recruitment writes five pieces of state at once. If one throws, **none** land:

```csharp
new GuardedEvent("mossu.recruit",
    new AllOf(
        new FlagSet(StateKeys.MossuDefeated),
        new FlagSet(StateKeys.MossuRecruited, false)),
    new SetFlag(StateKeys.MossuRecruited),
    new SetFlag(StateKeys.MossuInTown),
    new SetFlag(StateKeys.GreenhouseUnlocked),
    new SetCounter(StateKeys.GreenhouseLevel, 1),
    new AddCounter(StateKeys.ProsperityPoints, 1),
    new AddCounter(StateKeys.KiroHappiness, 10)),
```

Half-applying that produces a monster that is recruited but has no home. That
state survives a save file and is miserable to diagnose later, which is why the
rollback exists and why it is tested in red.

Re-firing is refused rather than applied twice. A double-fired recruit would
inflate prosperity and skip a whole town stage.

### Blocked events explain themselves

```csharp
var result = Ev(evs, "mossu.defeat").TryFire(s);
// result.DidFire == false
// result.Reason  == "World.Forest.GreenhouseMechanismFound"
```

And so does the world graph:

```csharp
graph.ExplainUnreachable(Town, DeepGrove, state)
// ["forest_riverside -> forest_overgrown_ruins needs
//   World.Forest.VegetationDamageFound"]
```

Without that, a failing progression test says `false` and the next half hour
goes into finding out which gate. With it, an agent reads the answer directly.

### Saves are plain text and deterministic

```
monstr-save v1
Creature.Mossu.LivesInTown=true
Creature.Mossu.Recruited=true
Settlement.Greenhouse.Level=1
Settlement.ProsperityPoints=1
...
```

Sorted keys, so two saves of the same state are byte-identical and `diff` shows
exactly what a step changed. Unknown or missing keys are **hard errors**, not
skips — silently dropping state is how progression bugs get blamed on gameplay
code.

---

## 4. Two mechanics borrowed from DW1, implemented from scratch

### Invertible requirements

DW1's evolution checks carry a flag bit that reverses the comparison, so some
evolutions require care mistakes *above* a number. One bit, and a neglect path
plus a devotion path share a single data table.

```csharp
new CounterThreshold(StateKeys.KiroBattles, 3, inverted: true)
// passes with 0 battles, fails with 10
```

Worth having from day one: retrofitting it means rewriting every requirement
row.

### Bands, not floors

DW1's weight check is a ±5 window, which is why overfeeding fails exactly like
starving. Better than a minimum and costs nothing:

```csharp
new CounterInBand(StateKeys.KiroWeight, target: 20, tolerance: 5)
// 14 fails, 20 passes, 25 passes, 26 fails
```

### Prosperity as one derived value

The single DW1 idea taken directly: `PSTAT_PROSPERITY_POINTS`. The blueprint §3
growth stages become thresholds on one integer rather than a dozen booleans,
which is far easier to tune.

```
0-2   Abandoned      13-18  Connected
3-7   Camp           19+    Living
8-12  Village
```

Verified across all ten boundary values.

---

## 5. Partner simulation: scope warning

The DW1 measurement found **65 fields** in `PartnerPara` — sleep schedule,
hunger timers, sickness counters, virus bar, tiredness, discipline, happiness,
weight, care mistakes, per-stat training counters, remaining lifetime. 53 named,
12 still unidentified.

That is a shipped game, not a vertical slice. The harness registers **13**:

```
Hunger  Tiredness  Happiness  Discipline  CareMistakes  Weight  Battles
Stage   Train.{Hp, Offense, Defense, Speed, Brain}
```

These are the subset that actually feeds an evolution decision. Add the rest
when something needs them, not before.

**The design implication is the real finding.** The mechanics plan §7 treats
DW1's evolution algorithm as the thing to study. It is ~150 lines of
comparisons. What makes evolution *feel* mysterious is the 65-field simulation
running underneath it. MONSTR needs a continuously-running partner simulation,
not a clever formula.

---

## 6. What this does NOT prove

Being explicit, because the result above is easy to over-read.

- **No combat.** The slice models the fight as a single state transition. Whether
  combat is *fun* is the actual risk in the project and no test can answer it.
- **No Unity integration.** This is a plain .NET 8 library. Consuming it from
  Unity 6 should be trivial (no external dependencies, no modern-only language
  features beyond records) but that is untested.
- **One monster.** Mossu only. The remaining 24 in the roster will surface
  patterns this does not.
- **No scene work.** Creating areas, placing markers and wiring transitions in
  an actual Unity scene is the part that needs the Editor and MCP.
- **Nothing about agent productivity.** The harness proves the work is
  *verifiable* headlessly. Whether an agent can usefully *do* it is a separate
  experiment.

---

## 7. What this changes about the infrastructure decision

The phased plan holds, with one adjustment: **Phase 2 is more valuable than it
looked.**

```
Phase 1  prototype          dev PC + GitHub                       ~EUR 0
Phase 2  agents/CI useful   Hetzner Cloud ~8 vCPU / 32 GB         ~EUR 20-50/mo
Phase 3  agents constant    dedicated 12-16 core / 64 GB / 1 TB   ~EUR 100+/mo
Phase 4  visual agent work  add GPU only if proven
```

A CPU-only box runs this suite in 95 ms. It can run the progression tests for
all five regions, validate the world graph for orphans and dangling exits, and
catch a broken gate before anyone opens the Editor. That is real CI value at the
cheapest tier.

**Still unresolved and blocking Phase 2:** the Unity licence. ToS §2.8 permits
the Editor on a primary *and* secondary machine but *"only one instance at any
given time per seat"*. An agent driving the Editor server-side while you work
locally is two concurrent instances. The Ansible role refuses to install Unity
until `unity_licence_confirmed=true` is set, deliberately.

Note that this harness **does not need that answer**. It is plain .NET and runs
anywhere today.

---

## 8. Reproduce it

```bash
cd port/monstr-core
export DOTNET_ROOT=/opt/dotnet PATH="$PATH:/opt/dotnet"
dotnet test
```

The red-guard proof:

```bash
bash scripts/prove-guards.sh
```

Infrastructure validation:

```bash
cd port/monstr-infra && bash scripts/validate.sh
```

---

## 9. Recommended next step

Not more infrastructure, and not the other 24 monsters.

**Wire this library into a Unity project and make the Mossu slice playable.**
The harness proves the progression is correct; it says nothing about whether
walking into that forest and fighting that monster is enjoyable. Blueprint §19
is right that if the loop is not fun, nothing else matters.

Two things worth doing alongside it, both cheap:

1. **Rename `Zero`.** Mega Man X's Zero is one of the most recognisable
   characters in the genre, and it is attached to a story-critical late-game
   monster. Name collisions elsewhere in the roster came back clean.
2. **Decide whether the character-sheet level gates are placeholder.** The
   mockups show `Lv. 5 → Lv. 16 → Lv. 28 → Lv. 42`, which contradicts mechanics
   plan §7's stats/care/training/relationship model. Anyone shown that sheet
   will read it as a promise.
