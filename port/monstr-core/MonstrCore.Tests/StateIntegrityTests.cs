using System;
using System.Linq;
using Xunit;
using MonstrCore;

namespace MonstrCore.Tests
{
    /// <summary>
    /// Tests for the machinery itself: the guards that stop a progression bug
    /// from becoming a silent save-file corruption.
    /// </summary>
    public class StateIntegrityTests
    {
        [Fact]
        public void UnregisteredFlagThrowsWithASuggestion()
        {
            var s = new GameState();
            // The realistic failure: a typo in a world-graph data file.
            var ex = Assert.Throws<UnknownStateKeyException>(
                () => s.GetFlag("Creature.Mosu.Recruited"));

            Assert.Contains("Creature.Mossu.Recruited", ex.Message);
        }

        [Fact]
        public void UnregisteredCounterThrows()
        {
            var s = new GameState();
            Assert.Throws<UnknownStateKeyException>(
                () => s.SetCounter("Settlement.Greenhouse.Levl", 1));
        }

        [Fact]
        public void CountersRefuseToGoNegative()
        {
            var s = new GameState();
            Assert.Throws<ArgumentOutOfRangeException>(
                () => s.SetCounter(StateKeys.ProsperityPoints, -1));
        }

        [Fact]
        public void AddCounterClampsAtZeroRatherThanThrowing()
        {
            var s = new GameState();
            // Gameplay deltas should not crash on underflow; direct sets should.
            new AddCounter(StateKeys.KiroHappiness, -50).Execute(s);
            Assert.Equal(0, s.GetCounter(StateKeys.KiroHappiness));
        }

        /// <summary>
        /// A recruitment writes several keys at once. If one throws, none must
        /// land: a monster that is "recruited" with no home survives a save and
        /// is miserable to diagnose later.
        /// </summary>
        [Fact]
        public void AFailedEventRollsBackEveryChange()
        {
            var s = new GameState();
            var bad = new GuardedEvent("broken",
                new AllOf(),
                new SetFlag(StateKeys.MossuRecruited),
                new AddCounter(StateKeys.ProsperityPoints, 1),
                new SetCounter("Settlement.Does.Not.Exist", 1));   // throws here

            Assert.Throws<EventExecutionException>(() => bad.TryFire(s));

            Assert.False(s.GetFlag(StateKeys.MossuRecruited));
            Assert.Equal(0, s.GetCounter(StateKeys.ProsperityPoints));
        }

        [Fact]
        public void SaveRejectsAnUnknownKey()
        {
            var good = SaveGame.Serialise(new GameState());
            var tampered = good + "Creature.Ghost.Recruited=true\n";

            // Silently skipping unknown keys is how progression bugs get blamed
            // on gameplay code.
            var ex = Assert.Throws<SaveFormatException>(() => SaveGame.Deserialise(tampered));
            Assert.Contains("unregistered", ex.Message);
        }

        [Fact]
        public void SaveRejectsAMissingKey()
        {
            var lines = SaveGame.Serialise(new GameState()).Split('\n').ToList();
            lines.RemoveAt(2);
            Assert.Throws<SaveFormatException>(
                () => SaveGame.Deserialise(string.Join("\n", lines)));
        }

        [Fact]
        public void SaveRejectsAWrongFormatVersion()
        {
            var text = SaveGame.Serialise(new GameState()).Replace("v1", "v99");
            var ex = Assert.Throws<SaveFormatException>(() => SaveGame.Deserialise(text));
            Assert.Contains("Migration is not implemented", ex.Message);
        }

        [Fact]
        public void SaveOutputIsDeterministic()
        {
            var a = new GameState();
            var b = new GameState();
            new SetFlag(StateKeys.MossuRecruited).Execute(a);
            new SetFlag(StateKeys.MossuRecruited).Execute(b);

            // Needed for `diff` to be a useful debugging tool and for agents to
            // assert on save content.
            Assert.Equal(SaveGame.Serialise(a), SaveGame.Serialise(b));
        }
    }

    /// <summary>
    /// The two mechanical patterns taken from the DW1 measurement, implemented
    /// from scratch against MONSTR's own model.
    /// </summary>
    public class ConditionPatternTests
    {
        [Fact]
        public void ThresholdWorksInTheNormalDirection()
        {
            var s = new GameState();
            var c = new CounterThreshold(StateKeys.KiroBattles, 5);

            Assert.False(c.Evaluate(s));
            s.SetCounter(StateKeys.KiroBattles, 5);
            Assert.True(c.Evaluate(s));
        }

        /// <summary>
        /// The invertible requirement: one bit gives a "neglect" path alongside
        /// a "devotion" path from the same data table.
        /// </summary>
        [Fact]
        public void InvertedThresholdRequiresBeingBelowTheValue()
        {
            var s = new GameState();
            var fewBattles = new CounterThreshold(StateKeys.KiroBattles, 3, inverted: true);

            Assert.True(fewBattles.Evaluate(s));          // 0 battles qualifies
            s.SetCounter(StateKeys.KiroBattles, 10);
            Assert.False(fewBattles.Evaluate(s));         // 10 does not

            Assert.Contains("<=", fewBattles.Describe());
        }

        /// <summary>
        /// Weight as a band, so overfeeding fails exactly like starving.
        /// </summary>
        [Fact]
        public void BandRejectsBothTooLowAndTooHigh()
        {
            var s = new GameState();
            var band = new CounterInBand(StateKeys.KiroWeight, target: 20, tolerance: 5);

            s.SetCounter(StateKeys.KiroWeight, 14);
            Assert.False(band.Evaluate(s));

            s.SetCounter(StateKeys.KiroWeight, 20);
            Assert.True(band.Evaluate(s));

            s.SetCounter(StateKeys.KiroWeight, 25);
            Assert.True(band.Evaluate(s));

            s.SetCounter(StateKeys.KiroWeight, 26);
            Assert.False(band.Evaluate(s));
        }

        [Fact]
        public void AllOfReportsWhichPartsFailed()
        {
            var s = new GameState();
            var gate = new AllOf(
                new FlagSet(StateKeys.MossuRecruited),
                new CounterThreshold(StateKeys.ProsperityPoints, 3));

            var failing = gate.FailingParts(s).ToList();
            Assert.Equal(2, failing.Count);

            s.SetFlag(StateKeys.MossuRecruited, true);
            Assert.Single(gate.FailingParts(s));
        }
    }

    /// <summary>
    /// Town growth driven by one derived integer, which is the single idea
    /// worth taking directly from DW1's PSTAT_PROSPERITY_POINTS.
    /// </summary>
    public class SettlementStageTests
    {
        [Theory]
        [InlineData(0,  SettlementStage.Abandoned)]
        [InlineData(2,  SettlementStage.Abandoned)]
        [InlineData(3,  SettlementStage.Camp)]
        [InlineData(7,  SettlementStage.Camp)]
        [InlineData(8,  SettlementStage.Village)]
        [InlineData(12, SettlementStage.Village)]
        [InlineData(13, SettlementStage.Connected)]
        [InlineData(18, SettlementStage.Connected)]
        [InlineData(19, SettlementStage.Living)]
        [InlineData(25, SettlementStage.Living)]
        public void StageFollowsProsperityThresholds(int prosperity, SettlementStage expected)
        {
            var s = new GameState();
            s.SetCounter(StateKeys.ProsperityPoints, prosperity);
            Assert.Equal(expected, s.Settlement.Stage);
        }

        [Fact]
        public void OneRecruitMovesTheTownOffAbandonedOnlyAtThreshold()
        {
            var s = new GameState();
            s.Settlement.AddProsperity(1);
            Assert.Equal(SettlementStage.Abandoned, s.Settlement.Stage);

            s.Settlement.AddProsperity(2);
            Assert.Equal(SettlementStage.Camp, s.Settlement.Stage);
        }
    }
}
