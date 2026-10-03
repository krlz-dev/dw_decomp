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
        //
        // FOUR distinct facts per monster, not one. The constitution is explicit:
        //
        //     Defeated != Befriended != LivesInTown
        //
        // Winning a fight creates an opportunity to communicate. It does not
        // transfer ownership. A monster can be defeated and not befriended,
        // befriended and living elsewhere, befriended and visiting, or
        // befriended and resident. Collapsing any of these into one flag would
        // delete the design pillar the whole game rests on, and would quietly
        // make "Recruited" mean "captured".
        //
        // The previous key name was Creature.Mossu.Recruited. "Recruited"
        // carries the collection-game assumption this project rejects, so the
        // vocabulary changed with the model.

        public const string MossuDefeated       = "Creature.Mossu.Defeated";
        public const string MossuBefriended     = "Creature.Mossu.Befriended";
        public const string MossuInvitedToTown  = "Creature.Mossu.InvitedToTown";
        public const string MossuLivesInTown    = "Creature.Mossu.LivesInTown";

        public const string EmberuDefeated      = "Creature.Emberu.Defeated";
        public const string EmberuBefriended    = "Creature.Emberu.Befriended";
        public const string EmberuInvitedToTown = "Creature.Emberu.InvitedToTown";
        public const string EmberuLivesInTown   = "Creature.Emberu.LivesInTown";

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
        public const string MainChapter        = "Story.MainChapter";
        public const string DormantNetworkSeen = "Story.DormantNetworkDiscovered";

        // ---- partner ---------------------------------------------------------
        // Minimum viable partner simulation. The DW1 measurement found 65 fields
        // in PartnerPara; that is the shipped game, not a vertical slice. These
        // thirteen are the subset that actually feeds a gameplay decision.
        public const string KiroHunger       = "Partner.Kiro.Hunger";
        public const string KiroTiredness    = "Partner.Kiro.Tiredness";
        public const string KiroHappiness    = "Partner.Kiro.Happiness";
        public const string KiroDiscipline   = "Partner.Kiro.Discipline";
        public const string KiroCareMistakes = "Partner.Kiro.CareMistakes";
        public const string KiroWeight       = "Partner.Kiro.Weight";
        public const string KiroBattles      = "Partner.Kiro.Battles";
        public const string KiroStage        = "Partner.Kiro.Stage";
        public const string KiroTrainHp      = "Partner.Kiro.Train.Hp";
        public const string KiroTrainOff     = "Partner.Kiro.Train.Offense";
        public const string KiroTrainDef     = "Partner.Kiro.Train.Defense";
        public const string KiroTrainSpeed   = "Partner.Kiro.Train.Speed";
        public const string KiroTrainBrain   = "Partner.Kiro.Train.Brain";

        public static readonly IReadOnlyCollection<string> AllFlags = new[]
        {
            MossuDefeated, MossuBefriended, MossuInvitedToTown, MossuLivesInTown,
            EmberuDefeated, EmberuBefriended, EmberuInvitedToTown, EmberuLivesInTown,
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

    /// <summary>
    /// The four relationship facts for one monster, grouped so gameplay code
    /// cannot accidentally treat them as interchangeable.
    ///
    /// This is a read model over the flat store, not a second source of truth.
    /// </summary>
    public sealed class Relationship
    {
        private readonly GameState _s;

        public string Name { get; }
        public string DefeatedKey { get; }
        public string BefriendedKey { get; }
        public string InvitedKey { get; }
        public string LivesInTownKey { get; }

        internal Relationship(GameState s, string name,
                              string defeated, string befriended,
                              string invited, string livesInTown)
        {
            _s = s;
            Name = name;
            DefeatedKey = defeated;
            BefriendedKey = befriended;
            InvitedKey = invited;
            LivesInTownKey = livesInTown;
        }

        public bool Defeated    => _s.GetFlag(DefeatedKey);
        public bool Befriended  => _s.GetFlag(BefriendedKey);
        public bool Invited     => _s.GetFlag(InvitedKey);
        public bool LivesInTown => _s.GetFlag(LivesInTownKey);

        /// <summary>
        /// The states the constitution says must stay reachable. Useful in tests
        /// and in a debug panel, and it names the situation rather than making
        /// the reader decode four booleans.
        /// </summary>
        public RelationshipStage Stage =>
            LivesInTown ? RelationshipStage.Resident
            : Invited    ? RelationshipStage.InvitedNotMoved
            : Befriended ? RelationshipStage.FriendElsewhere
            : Defeated   ? RelationshipStage.DefeatedNotFriend
            : RelationshipStage.Stranger;

        /// <summary>
        /// Guards the one ordering that is a design rule rather than a
        /// convenience: a monster cannot live in town without having been
        /// invited, and cannot be invited without being a friend. Defeat is
        /// NOT a prerequisite for friendship, deliberately: the roster has
        /// monsters who could plausibly befriend Rui without a fight.
        /// </summary>
        public bool IsCoherent()
        {
            if (LivesInTown && !Invited) return false;
            if (Invited && !Befriended) return false;
            return true;
        }
    }

    public enum RelationshipStage
    {
        Stranger,
        DefeatedNotFriend,
        FriendElsewhere,
        InvitedNotMoved,
        Resident,
    }
}
