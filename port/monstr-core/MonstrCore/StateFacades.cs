using System;

namespace MonstrCore
{
    // Typed façades over the flat store. These exist so gameplay code reads
    // like the design document rather than like dictionary access, while the
    // store underneath stays trivially serialisable.

    public sealed class CreatureState
    {
        private readonly GameState _s;
        internal CreatureState(GameState s) => _s = s;

        public Relationship Mossu => new(_s, "Mossu",
            StateKeys.MossuDefeated, StateKeys.MossuBefriended,
            StateKeys.MossuInvitedToTown, StateKeys.MossuLivesInTown);

        public Relationship Emberu => new(_s, "Emberu",
            StateKeys.EmberuDefeated, StateKeys.EmberuBefriended,
            StateKeys.EmberuInvitedToTown, StateKeys.EmberuLivesInTown);

        /// <summary>
        /// Every monster whose relationship is tracked. Lets a test assert the
        /// coherence invariant across all of them without listing names, so a
        /// monster added later is covered automatically.
        /// </summary>
        public Relationship[] All => new[] { Mossu, Emberu };
    }

    public sealed class WorldState
    {
        private readonly GameState _s;
        internal WorldState(GameState s) => _s = s;

        public bool Is(string flag) => _s.GetFlag(flag);
        public void Set(string flag) => _s.SetFlag(flag, true);
    }

    public sealed class SettlementState
    {
        private readonly GameState _s;
        internal SettlementState(GameState s) => _s = s;

        public int GreenhouseLevel => _s.GetCounter(StateKeys.GreenhouseLevel);
        public int Prosperity => _s.GetCounter(StateKeys.ProsperityPoints);

        public void SetGreenhouseLevel(int lvl) => _s.SetCounter(StateKeys.GreenhouseLevel, lvl);
        public void AddProsperity(int n) =>
            _s.SetCounter(StateKeys.ProsperityPoints, Prosperity + n);

        /// <summary>
        /// Blueprint section 3 growth stages as thresholds on one value, rather
        /// than a separate flag per stage.
        /// </summary>
        public SettlementStage Stage => Prosperity switch
        {
            < 3  => SettlementStage.Abandoned,
            < 8  => SettlementStage.Camp,
            < 13 => SettlementStage.Village,
            < 19 => SettlementStage.Connected,
            _    => SettlementStage.Living,
        };
    }

    public enum SettlementStage { Abandoned, Camp, Village, Connected, Living }

    public sealed class StoryState
    {
        private readonly GameState _s;
        internal StoryState(GameState s) => _s = s;

        public int Chapter => _s.GetCounter(StateKeys.MainChapter);
        public void SetChapter(int c) => _s.SetCounter(StateKeys.MainChapter, c);
    }

    public sealed class PartnerState
    {
        private readonly GameState _s;
        internal PartnerState(GameState s) => _s = s;

        public int Get(string key) => _s.GetCounter(key);
        public void Set(string key, int v) => _s.SetCounter(key, v);
        public void Add(string key, int delta) =>
            _s.SetCounter(key, Math.Max(0, Get(key) + delta));
    }
}
