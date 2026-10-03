using System.Collections.Generic;

namespace MonstrCore
{
    /// <summary>
    /// The vertical slice from the region blueprint section 19, expressed as
    /// data: Town -> Forest Entrance -> Riverside -> Overgrown Ruins -> Mossu.
    ///
    /// The recruitment is deliberately FOUR events, not one. The constitution
    /// requires Defeated != Befriended != LivesInTown, and the spike handoff
    /// section 16 allows them to happen close together in the slice while
    /// insisting they stay conceptually distinct. Modelling them as one step
    /// would make the architecture unable to express a monster who is
    /// befriended but lives elsewhere, which the roster already contains
    /// (Kiba stays in the canyon; Tsuki only visits).
    /// </summary>
    public static class MossuSlice
    {
        public const string Town            = "town_plaza";
        public const string ForestEntrance  = "forest_entrance";
        public const string Riverside       = "forest_riverside";
        public const string OvergrownRuins  = "forest_overgrown_ruins";
        public const string DeepGrove       = "forest_deep_grove";
        public const string CanyonCrossing  = "canyon_broken_crossing";

        public static WorldGraph BuildGraph()
        {
            var g = new WorldGraph();

            g.Add(new Area(Town, "Settlement Plaza")
                .Exit(ForestEntrance));

            g.Add(new Area(ForestEntrance, "Forest Entrance")
                .Exit(Town)
                .Exit(Riverside));

            g.Add(new Area(Riverside, "Riverside")
                .Exit(ForestEntrance)
                .Exit(OvergrownRuins,
                    // The player must notice the damaged vegetation before the
                    // ruins mean anything. Gating on a discovery rather than a
                    // level is blueprint rule 3.
                    new FlagSet(StateKeys.ForestVegetationDamageFound))
                // The canyon route is visible from here and shut, which is
                // blueprint rule 7: see the inaccessible place first.
                .Exit(CanyonCrossing,
                    new FlagSet(StateKeys.CanyonBridgeRepaired)));

            g.Add(new Area(OvergrownRuins, "Overgrown Ruins")
                .Exit(Riverside)
                .Exit(DeepGrove,
                    // Mossu has to actually choose to help, not merely lose.
                    // Gating on Befriended rather than Defeated is the whole
                    // point of keeping the two apart.
                    new FlagSet(StateKeys.MossuBefriended)));

            g.Add(new Area(DeepGrove, "Deep Grove")
                .Exit(OvergrownRuins));

            g.Add(new Area(CanyonCrossing, "Broken Crossing")
                .Exit(Riverside));

            return g;
        }

        // Event ids, so Unity adapters and tests refer to the same strings.
        public const string EvDiscoverDamage = "forest.discover_damage";
        public const string EvFindMechanism  = "forest.find_greenhouse_mechanism";
        public const string EvDefeatMossu    = "mossu.defeat";
        public const string EvBefriendMossu  = "mossu.befriend";
        public const string EvInviteMossu    = "mossu.invite";
        public const string EvMossuMovesIn   = "mossu.moves_in";

        /// <summary>
        /// The slice's events, in the order a player would trigger them. Each
        /// one is gated, so an out-of-order attempt reports what is missing
        /// instead of corrupting state.
        /// </summary>
        public static IReadOnlyList<GuardedEvent> BuildEvents() => new[]
        {
            // 1. Discovery: the forest is sick.
            new GuardedEvent(EvDiscoverDamage,
                new AllOf(),  // always available on arrival
                new SetFlag(StateKeys.ForestVegetationDamageFound)),

            // 2. Follow it to the greenhouse mechanism Mossu is guarding.
            //    This is what makes the fight understandable before it starts,
            //    per handoff section 12.
            new GuardedEvent(EvFindMechanism,
                new FlagSet(StateKeys.ForestVegetationDamageFound),
                new SetFlag(StateKeys.ForestGreenhouseMechFound)),

            // 3. The fight. Kiro's battle count rises; nothing else changes.
            //    Handoff section 15: combat ending must not become joining.
            new GuardedEvent(EvDefeatMossu,
                new AllOf(
                    new FlagSet(StateKeys.ForestGreenhouseMechFound),
                    new FlagSet(StateKeys.MossuDefeated, false)),
                new SetFlag(StateKeys.MossuDefeated),
                new AddCounter(StateKeys.KiroBattles, 1)),

            // 4. The negotiation. Rui asks about the forest and offers to
            //    restore the settlement greenhouse; Mossu decides. Winning the
            //    fight only earned the conversation.
            new GuardedEvent(EvBefriendMossu,
                new AllOf(
                    new FlagSet(StateKeys.MossuDefeated),
                    new FlagSet(StateKeys.MossuBefriended, false)),
                new SetFlag(StateKeys.MossuBefriended),
                new AddCounter(StateKeys.KiroHappiness, 10)),

            // 5. The invitation. Separate because a friend who declines to
            //    move is a state the roster needs (Kiba, Tsuki).
            new GuardedEvent(EvInviteMossu,
                new AllOf(
                    new FlagSet(StateKeys.MossuBefriended),
                    new FlagSet(StateKeys.MossuInvitedToTown, false)),
                new SetFlag(StateKeys.MossuInvitedToTown)),

            // 6. Mossu actually arrives, and the world changes. This is the
            //    only event that touches the settlement: handoff section 17
            //    insists the town is the progression interface.
            new GuardedEvent(EvMossuMovesIn,
                new AllOf(
                    new FlagSet(StateKeys.MossuInvitedToTown),
                    new FlagSet(StateKeys.MossuLivesInTown, false)),
                new SetFlag(StateKeys.MossuLivesInTown),
                new SetFlag(StateKeys.GreenhouseUnlocked),
                new SetCounter(StateKeys.GreenhouseLevel, 1),
                // Prosperity is the single derived value driving town stage,
                // borrowed from DW1's PSTAT_PROSPERITY_POINTS.
                new AddCounter(StateKeys.ProsperityPoints, 1)),
        };

        /// <summary>
        /// The happy path, for adapters and tests that just need the sequence.
        /// </summary>
        public static readonly string[] HappyPath =
        {
            EvDiscoverDamage, EvFindMechanism, EvDefeatMossu,
            EvBefriendMossu, EvInviteMossu, EvMossuMovesIn,
        };
    }
}
