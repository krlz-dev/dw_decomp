using System;
using System.Collections.Generic;
using System.Linq;
using Xunit;
using MonstrCore;

namespace MonstrCore.Tests
{
    /// <summary>
    /// The region blueprint section 19 checklist, as tests.
    ///
    /// Every one of its items through "save, quit/reload, state remains
    /// correct" is verified here WITHOUT Unity, without a renderer and without
    /// a GPU. That is the measurement that decides whether a CPU-only box is
    /// worth paying for.
    /// </summary>
    public class VerticalSliceTests
    {
        private static (GameState state, WorldGraph graph, IReadOnlyList<GuardedEvent> events) NewGame()
            => (new GameState(), MossuSlice.BuildGraph(), MossuSlice.BuildEvents());

        private static GuardedEvent Ev(IReadOnlyList<GuardedEvent> evs, string id)
            => evs.Single(e => e.Id == id);

        private static void Play(GameState s, IReadOnlyList<GuardedEvent> evs, params string[] ids)
        {
            foreach (var id in ids)
            {
                var r = Ev(evs, id).TryFire(s);
                Assert.True(r.DidFire, $"'{id}' was blocked: {r.Reason}");
            }
        }

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
            // The diagnostic must name the blocking requirement, not just
            // report false. Handoff section 11 requires this.
            Assert.Contains(why, w => w.Contains(StateKeys.ForestVegetationDamageFound));
        }

        // ---- events are ordered and refuse to run early ---------------------

        [Fact]
        public void TheFightIsBlockedBeforeTheDiscovery()
        {
            var (s, _, evs) = NewGame();
            var result = Ev(evs, MossuSlice.EvDefeatMossu).TryFire(s);

            Assert.False(result.DidFire);
            Assert.Contains(StateKeys.ForestGreenhouseMechFound, result.Reason);
            Assert.False(s.Creatures.Mossu.Defeated);
            Assert.Equal(0, s.GetCounter(StateKeys.KiroBattles));
        }

        [Fact]
        public void FriendshipIsBlockedBeforeTheFight()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism);

            var result = Ev(evs, MossuSlice.EvBefriendMossu).TryFire(s);

            Assert.False(result.DidFire);
            Assert.False(s.Creatures.Mossu.Befriended);
        }

        [Fact]
        public void MovingInIsBlockedWithoutAnInvitation()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu, MossuSlice.EvBefriendMossu);

            var result = Ev(evs, MossuSlice.EvMossuMovesIn).TryFire(s);

            Assert.False(result.DidFire);
            Assert.False(s.Creatures.Mossu.LivesInTown);
            // And the town has not changed.
            Assert.False(s.GetFlag(StateKeys.GreenhouseUnlocked));
            Assert.Equal(0, s.Settlement.Prosperity);
        }

        // ---- the constitutional invariant: four distinct facts --------------

        /// <summary>
        /// Handoff section 4: Defeated != Befriended != LivesInTown.
        /// Winning a fight creates an opportunity to communicate. It does not
        /// transfer ownership of the monster.
        /// </summary>
        [Fact]
        public void WinningTheFightDoesNotCreateFriendship()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu);

            var mossu = s.Creatures.Mossu;
            Assert.True(mossu.Defeated);
            Assert.False(mossu.Befriended);
            Assert.False(mossu.Invited);
            Assert.False(mossu.LivesInTown);
            Assert.Equal(RelationshipStage.DefeatedNotFriend, mossu.Stage);

            // Nothing about the settlement moved.
            Assert.Equal(0, s.Settlement.Prosperity);
            Assert.Equal(0, s.Settlement.GreenhouseLevel);
        }

        /// <summary>
        /// A friend who has not been invited, and an invited friend who has not
        /// yet moved, are both legal states. The roster needs them: Kiba stays
        /// in the canyon until the rivalry matures, and Tsuki only visits.
        /// </summary>
        [Fact]
        public void FriendshipDoesNotImplyLivingInTown()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu, MossuSlice.EvBefriendMossu);

            var mossu = s.Creatures.Mossu;
            Assert.True(mossu.Befriended);
            Assert.False(mossu.Invited);
            Assert.False(mossu.LivesInTown);
            Assert.Equal(RelationshipStage.FriendElsewhere, mossu.Stage);
        }

        [Fact]
        public void AnInvitedFriendNeedNotHaveMovedYet()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu, MossuSlice.EvBefriendMossu,
                         MossuSlice.EvInviteMossu);

            var mossu = s.Creatures.Mossu;
            Assert.True(mossu.Invited);
            Assert.False(mossu.LivesInTown);
            Assert.Equal(RelationshipStage.InvitedNotMoved, mossu.Stage);
        }

        [Fact]
        public void EveryTrackedRelationshipStaysCoherent()
        {
            var (s, _, evs) = NewGame();
            foreach (var id in MossuSlice.HappyPath)
            {
                Ev(evs, id).TryFire(s);
                foreach (var r in s.Creatures.All)
                    Assert.True(r.IsCoherent(),
                        $"{r.Name} reached an incoherent state after '{id}': {r.Stage}");
            }
        }

        // ---- the full chain -------------------------------------------------

        [Fact]
        public void TheFullSliceChainCompletes()
        {
            var (s, g, evs) = NewGame();
            Play(s, evs, MossuSlice.HappyPath);

            var mossu = s.Creatures.Mossu;
            Assert.Equal(RelationshipStage.Resident, mossu.Stage);

            // Friendship changed the world, which is the whole point.
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
        public void TheGroveOpensOnFriendshipNotOnVictory()
        {
            var (s, g, evs) = NewGame();
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu);

            // Beating Mossu is not enough: she has to choose to help.
            Assert.DoesNotContain(MossuSlice.DeepGrove, g.Reachable(MossuSlice.Town, s));

            Play(s, evs, MossuSlice.EvBefriendMossu);
            Assert.Contains(MossuSlice.DeepGrove, g.Reachable(MossuSlice.Town, s));
        }

        [Fact]
        public void EventsAreIdempotentAndCannotDoubleCount()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.HappyPath);

            var prosperity = s.Settlement.Prosperity;
            var battles = s.GetCounter(StateKeys.KiroBattles);

            // Re-firing must be refused, not applied twice. A double-fired
            // move-in would inflate prosperity and skip a town stage.
            foreach (var id in MossuSlice.HappyPath)
                if (id != MossuSlice.EvDiscoverDamage && id != MossuSlice.EvFindMechanism)
                    Assert.False(Ev(evs, id).TryFire(s).DidFire, $"'{id}' fired twice");

            Assert.Equal(prosperity, s.Settlement.Prosperity);
            Assert.Equal(battles, s.GetCounter(StateKeys.KiroBattles));
        }

        // ---- save and reload ------------------------------------------------

        [Fact]
        public void SaveReloadPreservesStateExactly()
        {
            var (s, _, evs) = NewGame();
            Play(s, evs, MossuSlice.HappyPath);

            var saved = SaveGame.Serialise(s);
            var loaded = SaveGame.Deserialise(saved);

            Assert.Equal(saved, SaveGame.Serialise(loaded));
            Assert.Equal(RelationshipStage.Resident, loaded.Creatures.Mossu.Stage);
            Assert.Equal(1, loaded.Settlement.GreenhouseLevel);
            Assert.Equal(1, loaded.Settlement.Prosperity);
        }

        [Fact]
        public void ReloadedStateStillGatesCorrectly()
        {
            var (s, g, evs) = NewGame();
            Play(s, evs, MossuSlice.HappyPath);

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
            Play(s, evs, MossuSlice.EvDiscoverDamage, MossuSlice.EvFindMechanism,
                         MossuSlice.EvDefeatMossu);

            var loaded = SaveGame.Deserialise(SaveGame.Serialise(s));
            var evs2 = MossuSlice.BuildEvents();

            // The negotiation must still be available after a reload, and
            // moving in still blocked.
            Assert.True(Ev(evs2, MossuSlice.EvBefriendMossu).CanFire(loaded));
            Assert.False(Ev(evs2, MossuSlice.EvMossuMovesIn).CanFire(loaded));
        }

        /// <summary>
        /// Handoff section 18 lists the keys that must persist. Asserting on the
        /// list directly means removing one from StateKeys fails a test rather
        /// than silently shrinking the save.
        /// </summary>
        [Fact]
        public void SaveContainsEveryKeyTheHandoffRequires()
        {
            var text = SaveGame.Serialise(new GameState());
            foreach (var key in new[]
            {
                StateKeys.MossuDefeated, StateKeys.MossuBefriended,
                StateKeys.MossuInvitedToTown, StateKeys.MossuLivesInTown,
                StateKeys.GreenhouseLevel, StateKeys.ProsperityPoints,
                StateKeys.KiroBattles, StateKeys.KiroHappiness,
                StateKeys.ForestVegetationDamageFound,
            })
                Assert.Contains(key + "=", text);
        }
    }
}
