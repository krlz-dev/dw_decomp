using System.Collections.Generic;

namespace MonstrCore
{
    /// <summary>
    /// The registry of every legal state key, using the naming convention from
    /// the region blueprint section 15.
    ///
    /// One place, so a designer or an agent can read the whole progression
    /// surface without grepping scene files. This is also what makes the typo
    /// guard in GameState possible.
    /// </summary>
    public static class StateKeys
    {
        // ---- creatures -------------------------------------------------------
        // Two separate flags per monster on purpose. The roster document is
        // explicit that winning a fight does NOT mean recruitment: the monster
        // decides afterwards. Collapsing these into one flag would quietly
        // delete that design pillar.
        public const string MossuDefeated  = "Creature.Mossu.Defeated";
        public const string MossuRecruited = "Creature.Mossu.Recruited";
        public const string MossuInTown    = "Creature.Mossu.LivesInTown";

        public const string EmberuDefeated  = "Creature.Emberu.Defeated";
        public const string EmberuRecruited = "Creature.Emberu.Recruited";
        public const string EmberuInTown    = "Creature.Emberu.LivesInTown";

        // ---- world -----------------------------------------------------------
        public const string ForestVegetationDamageFound = "World.Forest.VegetationDamageFound";
        public const string ForestGreenhouseMechFound   = "World.Forest.GreenhouseMechanismFound";
        public const string ForestDeepGroveCleared      = "World.Forest.DeepGroveCleared";
        public const string CanyonBridgeRepaired        = "World.Canyon.BridgeRepaired";

        // ---- settlement ------------------------------------------------------
        public const string GreenhouseUnlocked = "Settlement.Greenhouse.Unlocked";
        public const string GreenhouseLevel    = "Settlement.Greenhouse.Level";
        public const string TrainingYardLevel  = "Settlement.TrainingYard.Level";

        /// <summary>
        /// Single derived integer driving town growth, borrowed from the one DW1
        /// idea worth taking directly: PSTAT_PROSPERITY_POINTS. Cheaper to tune
        /// than a dozen booleans, and the growth stages in blueprint section 3
        /// become thresholds on it.
        /// </summary>
        public const string ProsperityPoints = "Settlement.ProsperityPoints";

        // ---- story -----------------------------------------------------------
        public const string MainChapter          = "Story.MainChapter";
        public const string DormantNetworkSeen   = "Story.DormantNetworkDiscovered";

        // ---- partner ---------------------------------------------------------
        // Minimum viable partner simulation. The DW1 measurement found 65 fields
        // in PartnerPara; that is the shipped game, not a vertical slice. These
        // thirteen are the subset that actually feeds evolution decisions.
        public const string KiroHunger      = "Partner.Kiro.Hunger";
        public const string KiroTiredness   = "Partner.Kiro.Tiredness";
        public const string KiroHappiness   = "Partner.Kiro.Happiness";
        public const string KiroDiscipline  = "Partner.Kiro.Discipline";
        public const string KiroCareMistakes= "Partner.Kiro.CareMistakes";
        public const string KiroWeight      = "Partner.Kiro.Weight";
        public const string KiroBattles     = "Partner.Kiro.Battles";
        public const string KiroStage       = "Partner.Kiro.Stage";
        public const string KiroTrainHp     = "Partner.Kiro.Train.Hp";
        public const string KiroTrainOff    = "Partner.Kiro.Train.Offense";
        public const string KiroTrainDef    = "Partner.Kiro.Train.Defense";
        public const string KiroTrainSpeed  = "Partner.Kiro.Train.Speed";
        public const string KiroTrainBrain  = "Partner.Kiro.Train.Brain";

        public static readonly IReadOnlyCollection<string> AllFlags = new[]
        {
            MossuDefeated, MossuRecruited, MossuInTown,
            EmberuDefeated, EmberuRecruited, EmberuInTown,
            ForestVegetationDamageFound, ForestGreenhouseMechFound,
            ForestDeepGroveCleared, CanyonBridgeRepaired,
            GreenhouseUnlocked,
            DormantNetworkSeen,
        };

        public static readonly IReadOnlyCollection<string> AllCounters = new[]
        {
            GreenhouseLevel, TrainingYardLevel, ProsperityPoints,
            MainChapter,
            KiroHunger, KiroTiredness, KiroHappiness, KiroDiscipline,
            KiroCareMistakes, KiroWeight, KiroBattles, KiroStage,
            KiroTrainHp, KiroTrainOff, KiroTrainDef, KiroTrainSpeed, KiroTrainBrain,
        };
    }

    // ---- typed façades over the flat store ----------------------------------
    // These exist so gameplay code reads like the design document rather than
    // like dictionary access, while the store underneath stays trivially
    // serialisable.

    public sealed class CreatureState
    {
        private readonly GameState _s;
        internal CreatureState(GameState s) => _s = s;

        public bool IsDefeated(string flag) => _s.GetFlag(flag);
        public bool IsRecruited(string flag) => _s.GetFlag(flag);
        public void MarkDefeated(string flag) => _s.SetFlag(flag, true);
        public void MarkRecruited(string flag) => _s.SetFlag(flag, true);
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
            _s.SetCounter(key, System.Math.Max(0, Get(key) + delta));
    }
}
