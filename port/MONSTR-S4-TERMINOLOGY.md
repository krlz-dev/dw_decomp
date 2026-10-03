# §4 Domain-Language Cleanup — Done

> **Handoff item:** section 4, first in the section 31 work order.
> **Status:** complete and verified. 42 tests green, all six guards proven red.
> **Scope:** terminology and model only. No Unity work, no gameplay, no new
> content.

---

## What changed

`Creature.Mossu.Recruited` is gone. The model now carries the four facts the
constitution requires:

```
Creature.Mossu.Defeated
Creature.Mossu.Befriended
Creature.Mossu.InvitedToTown
Creature.Mossu.LivesInTown
```

Same for Emberu, so the pattern is established rather than special-cased.

"Recruited" carried the collection-game assumption this project rejects. The
vocabulary changed because the model changed, not for cosmetics.

## The four states are reachable, and named

`Relationship.Stage` reports the situation instead of making the reader decode
four booleans:

| Stage | Meaning |
|---|---|
| `Stranger` | not met, or met without conflict |
| `DefeatedNotFriend` | lost the fight, chose not to help |
| `FriendElsewhere` | friend, still living in its own region |
| `InvitedNotMoved` | invited, has not come yet |
| `Resident` | living in the settlement |

`FriendElsewhere` is not hypothetical. The roster needs it: **Kiba** stays in the
canyon until the rivalry matures, **Tsuki** only visits. The old single flag
could not express either.

## One ordering rule, and one deliberate non-rule

`Relationship.IsCoherent()` enforces:

```
LivesInTown  requires  Invited
Invited      requires  Befriended
```

It deliberately does **not** require `Defeated` before `Befriended`. The roster
contains monsters who could plausibly befriend Rui without a fight, and baking
"combat first" into the invariant would close that door. Flagging this as a
judgement call rather than hiding it.

## The slice is now six events, not four

```
forest.discover_damage
forest.find_greenhouse_mechanism
mossu.defeat                 -> Defeated, Kiro battles +1
mossu.befriend               -> Befriended, Kiro happiness +10
mossu.invite                 -> InvitedToTown
mossu.moves_in               -> LivesInTown, greenhouse L1, prosperity +1
```

Only the last one touches the settlement, per handoff §17: the town is the
progression interface, so the town changes when Mossu actually arrives.

**One design change worth surfacing:** the Deep Grove exit now gates on
`Befriended`, not `Defeated`. Beating Mossu no longer opens the path; she has to
choose to help. That is a stricter reading of the constitution than the previous
version, and there is a test for it.

## Verification

```
baseline                                    Failed: 0, Passed: 42
remove atomic rollback                      Failed: 1, Passed: 41
allow unregistered keys silently            Failed: 1, Passed: 41
collapse DEFEAT into BEFRIEND               Failed: 10, Passed: 32   <-- §4 criterion
collapse BEFRIEND into LIVES-IN-TOWN        Failed: 7, Passed: 35
ungate the ruins exit                       Failed: 2, Passed: 40
grove opens on DEFEAT instead of friendship Failed: 1, Passed: 41
restored                                    Failed: 0, Passed: 42
```

**The §4 acceptance criterion is met, and the protection got stronger.** The old
proof caught a defeat/friendship collapse with 3 failing tests. It now catches
it with **10**, and there are two new collapses it also catches that the
previous model could not express.

Tests went 36 → 42. Run time 98 ms. Still no Unity, no renderer, no GPU.

```bash
cd port/monstr-core && dotnet test
cd port/monstr-core && bash scripts/prove-guards.sh
```

## Constitutional compliance

| Rule | Status |
|---|---|
| Monsters are characters, not collectibles | **PASS** — no ownership state exists |
| Fight does not mean friendship | **PASS** — enforced by 10 tests |
| `Defeated != Befriended != LivesInTown` | **PASS** — four keys, invariant tested |
| State explicit and testable | **PASS** — registry unchanged |
| Prefer reusable mechanics | **PASS** — `Relationship` is per-monster, not Mossu-specific |
| Do not rewrite the proven core | **PASS** — every §5 property intact |
| No scope expansion | **PASS** — no new regions, monsters or systems |

## Decisions requested

1. **Is `Defeated` ever a prerequisite for `Befriended`?** I left it optional so
   a no-combat friendship stays possible. If every friendship must be earned
   through a fight, say so and I will add the invariant.

2. **Should `mossu.invite` and `mossu.moves_in` be one player action in the
   slice?** §16 permits them close together. They are separate events now; the
   Unity adapter can fire both from one dialogue choice. Flagging it so the
   negotiation UI is designed knowingly.

## What this does not touch

Unity integration, Rui movement, Kiro follow, combat, the blockout, the
negotiation UI. §4 was the one item executable and verifiable headlessly, and it
is first in the §31 order. Everything from §7 onward needs the Editor.
