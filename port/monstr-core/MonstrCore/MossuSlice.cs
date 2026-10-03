using System.Collections.Generic;

namespace MonstrCore
{
    /// <summary>
    /// The vertical slice from the region blueprint section 19, expressed as
    /// data: Town -> Forest Entrance -> Riverside -> Overgrown Ruins -> Mossu.
    ///
    /// Note the shape of the recruitment: Defeated and Recruited are separate
    /// events with separate gates. The roster document's core rule is that
    /// winning a fight does not mean the monster joins; Mossu decides after a
    /// conversation. Modelling that as one step would quietly delete the pillar
    /// the whole game is built on.
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
                    // Mossu has to actually join, not merely lose.
                    new FlagSet(StateKeys.MossuRecruited)));

            g.Add(new Area(DeepGrove, "Deep Grove")
                .Exit(OvergrownRuins));

            g.Add(new Area(CanyonCrossing, "Broken Crossing")
                .Exit(Riverside));

            return g;
        }

        /// <summary>
        /// The slice's events, in the order a player would trigger them. Each
        /// one is gated, so an out-of-order attempt reports what is missing
        /// instead of corrupting state.
        /// </summary>
        public static IReadOnlyList<GuardedEvent> BuildEvents() => new[]
        {
            // 1. Discovery: the forest is sick.
            new GuardedEvent("forest.discover_damage",
                new AllOf(),  // always available on arrival
                new SetFlag(StateKeys.ForestVegetationDamageFound)),

            // 2. Follow it to the greenhouse mechanism Mossu is guarding.
            new GuardedEvent("forest.find_greenhouse_mechanism",
                new FlagSet(StateKeys.ForestVegetationDamageFound),
                new SetFlag(StateKeys.ForestGreenhouseMechFound)),

            // 3. The fight. Kiro's battle count goes up whatever the outcome.
            new GuardedEvent("mossu.defeat",
                new AllOf(
                    new FlagSet(StateKeys.ForestGreenhouseMechFound),
                    new FlagSet(StateKeys.MossuDefeated, false)),
                new SetFlag(StateKeys.MossuDefeated),
                new AddCounter(StateKeys.KiroBattles, 1)),

            // 4. The negotiation. Separate from the fight on purpose: Rui offers
            //    to restore the settlement greenhouse, and Mossu chooses.
            new GuardedEvent("mossu.recruit",
                new AllOf(
                    new FlagSet(StateKeys.MossuDefeated),
                    new FlagSet(StateKeys.MossuRecruited, false)),
                new SetFlag(StateKeys.MossuRecruited),
                new SetFlag(StateKeys.MossuInTown),
                new SetFlag(StateKeys.GreenhouseUnlocked),
                new SetCounter(StateKeys.GreenhouseLevel, 1),
                // Prosperity is the single derived value driving town stage,
                // borrowed from DW1's PSTAT_PROSPERITY_POINTS.
                new AddCounter(StateKeys.ProsperityPoints, 1),
                new AddCounter(StateKeys.KiroHappiness, 10)),
        };
    }
}
