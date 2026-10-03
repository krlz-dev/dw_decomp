using System;
using System.Linq;
using Xunit;
using MonstrCore;

namespace MonstrCore.Tests
{
    /// <summary>
    /// The region blueprint section 19 checklist, as tests.
    ///
    /// Every one of its 13 items through "save, quit/reload, state remains
    /// correct" is verified here WITHOUT Unity, without a renderer and without a
    /// GPU. That is the measurement that decides whether a CPU-only Hetzner box
    /// is worth paying for.
    /// </summary>
    public class VerticalSliceTests
    {
        private static (GameState state, WorldGraph graph, System.Collections.Generic.IReadOnlyList<GuardedEvent> events) NewGame()
            => (new GameState(), MossuSlice.BuildGraph(), MossuSlice.BuildEvents());

        private static GuardedEvent Ev(System.Collections.Generic.IReadOnlyList<GuardedEvent> evs, string id)
            => evs.Single(e => e.Id == id);

        // ---- the graph itself is well-formed --------------------------------

        [Fact]
        public void GraphHasNoDanglingExits()
        {
            var (_, g, _) = NewGame();
            Assert.Empty(g.FindDanglingExits());
        }

        [Fact]
        public void GraphHasNoOrphanAreas()
        {
            var (_, g, _) = NewGame();
            // Structural: with all gates open, every area must be reachable.
            // Catches an area wired into data with no path to it.
            Assert.Empty(g.FindOrphans(MossuSlice.Town));
        }

        // ---- the gates hold at the start -----------------------------------

        [Fact]
        public void AtStartOnlyTheForestEntranceIsOpen()
        {
            var (s, g, _) = NewGame();
            var reachable = g.Reachable(MossuSlice.Town, s);

            Assert.Contains(MossuSlice.Town, reachable);
            Assert.Contains(MossuSlice.ForestEntrance, reachable);
            Assert.Contains(MossuSlice.Riverside, reachable);

            // Gated until the player notices the damaged vegetation.
            Assert.DoesNotContain(MossuSlice.OvergrownRuins, reachable);
            Assert.DoesNotContain(MossuSlice.DeepGrove, reachable);
            // Blueprint rule 7: visible but shut.
            Assert.DoesNotContain(MossuSlice.CanyonCrossing, reachable);
        }

        [Fact]
        public void UnreachableTargetsExplainWhichGateIsShut()
        {
            var (s, g, _) = NewGame();
            var why = g.ExplainUnreachable(MossuSlice.Town, MossuSlice.DeepGrove, s);

            Assert.NotEmpty(why);
            // The diagnostic must name the actual blocking requirement, not
            // just report false.
            Assert.Contains(why, w => w.Contains(StateKeys.ForestVegetationDamageFound));
        }

        // ---- events are ordered and refuse to run early ---------------------

        [Fact]
        public void TheFightIsBlockedBeforeTheDiscovery()
        {
            var (s, _, evs) = NewGame();
            var result = Ev(evs, "mossu.defeat").TryFire(s);

            Assert.False(result.DidFire);
            Assert.Contains(StateKeys.ForestGreenhouseMechFound, result.Reason);
            // And nothing leaked.
            Assert.False(s.GetFlag(StateKeys.MossuDefeated));
            Assert.Equal(0, s.GetCounter(StateKeys.KiroBattles));
        }

        [Fact]
        public void RecruitmentIsBlockedBeforeTheFight()
        {
            var (s, _, evs) = NewGame();
            Ev(evs, "forest.discover_damage").TryFire(s);
            Ev(evs, "forest.find_greenhouse_mechanism").TryFire(s);

            var result = Ev(evs, "mossu.recruit").TryFire(s);

            Assert.False(result.DidFire);
            Assert.False(s.GetFlag(StateKeys.MossuRecruited));
            Assert.False(s.GetFlag(StateKeys.GreenhouseUnlocked));
        }

        /// <summary>
        /// The design pillar, as an executable assertion: winning the fight does
        /// NOT make Mossu join. The monster decides afterwards.
        /// </summary>
        [Fact]
        public void WinningTheFightDoesNotRecruitTheMonster()
        {
            var (s, _, evs) = NewGame();
            Ev(evs, "forest.discover_damage").TryFire(s);
            Ev(evs, "forest.find_greenhouse_mechanism").TryFire(s);
            Ev(evs, "mossu.defeat").TryFire(s);

            Assert.True(s.GetFlag(StateKeys.MossuDefeated));
            Assert.False(s.GetFlag(StateKeys.MossuRecruited));
            Assert.False(s.GetFlag(StateKeys.MossuInTown));
            Assert.Equal(0, s.GetCounter(StateKeys.ProsperityPoints));
        }

        // ---- the full chain -------------------------------------------------

        [Fact]
        public void TheFullSliceChainCompletes()
        {
            var (s, g, evs) = NewGame();

            foreach (var id in new[]
            {
                "forest.discover_damage",
                "forest.find_greenhouse_mechanism",
                "mossu.defeat",
                "mossu.recruit",
            })
            {
                var r = Ev(evs, id).TryFire(s);
                Assert.True(r.DidFire, $"'{id}' was blocked: {r.Reason}");
            }

            // Recruitment changed the world, which is the whole point.
            Assert.True(s.GetFlag(StateKeys.MossuRecruited));
            Assert.True(s.GetFlag(StateKeys.MossuInTown));
            Assert.True(s.GetFlag(StateKeys.GreenhouseUnlocked));
            Assert.Equal(1, s.Settlement.GreenhouseLevel);
            Assert.Equal(1, s.Settlement.Prosperity);
            Assert.Equal(1, s.GetCounter(StateKeys.KiroBattles));

            // And it opened new exploration.
            var reachable = g.Reachable(MossuSlice.Town, s);
            Assert.Contains(MossuSlice.DeepGrove, reachable);

            // But not the canyon, which needs its own bridge repair.
            Assert.DoesNotContain(MossuSlice.CanyonCrossing, reachable);
        }

        [Fact]
        public void EventsAreIdempotentAndCannotDoubleCount()
        {
            var (s, _, evs) = NewGame();
            Ev(evs, "forest.discover_damage").TryFire(s);
            Ev(evs, "forest.find_greenhouse_mechanism").TryFire(s);
            Ev(evs, "mossu.defeat").TryFire(s);
            Ev(evs, "mossu.recruit").TryFire(s);

            var prosperity = s.Settlement.Prosperity;
            var battles = s.GetCounter(StateKeys.KiroBattles);

            // Re-firing must be refused, not applied twice. A double-fired
            // recruit would inflate prosperity and skip a town stage.
            Assert.False(Ev(evs, "mossu.defeat").TryFire(s).DidFire);
            Assert.False(Ev(evs, "mossu.recruit").TryFire(s).DidFire);

            Assert.Equal(prosperity, s.Settlement.Prosperity);
            Assert.Equal(battles, s.GetCounter(StateKeys.KiroBattles));
        }

        // ---- save and reload ------------------------------------------------

        [Fact]
        public void SaveReloadPreservesStateExactly()
        {
            var (s, _, evs) = NewGame();
            foreach (var id in new[] { "forest.discover_damage", "forest.find_greenhouse_mechanism", "mossu.defeat", "mossu.recruit" })
                Ev(evs, id).TryFire(s);

            var saved = SaveGame.Serialise(s);
            var loaded = SaveGame.Deserialise(saved);

            // Byte-identical round trip, which also means a save diff shows
            // exactly what a step changed.
            Assert.Equal(saved, SaveGame.Serialise(loaded));
            Assert.True(loaded.GetFlag(StateKeys.MossuRecruited));
            Assert.Equal(1, loaded.Settlement.GreenhouseLevel);
            Assert.Equal(1, loaded.Settlement.Prosperity);
        }

        [Fact]
        public void ReloadedStateStillGatesCorrectly()
        {
            var (s, g, evs) = NewGame();
            foreach (var id in new[] { "forest.discover_damage", "forest.find_greenhouse_mechanism", "mossu.defeat", "mossu.recruit" })
                Ev(evs, id).TryFire(s);

            var loaded = SaveGame.Deserialise(SaveGame.Serialise(s));
            var reachable = g.Reachable(MossuSlice.Town, loaded);

            // The checklist's final item: quit, reload, state remains correct.
            Assert.Contains(MossuSlice.DeepGrove, reachable);
            Assert.DoesNotContain(MossuSlice.CanyonCrossing, reachable);
        }

        [Fact]
        public void SaveFromAnUnfinishedRunResumesMidChain()
        {
            var (s, _, evs) = NewGame();
            Ev(evs, "forest.discover_damage").TryFire(s);
            Ev(evs, "forest.find_greenhouse_mechanism").TryFire(s);

            var loaded = SaveGame.Deserialise(SaveGame.Serialise(s));
            var evs2 = MossuSlice.BuildEvents();

            // The fight must still be available after a reload, and the
            // recruitment still blocked.
            Assert.True(Ev(evs2, "mossu.defeat").CanFire(loaded));
            Assert.False(Ev(evs2, "mossu.recruit").CanFire(loaded));
        }
    }
}
