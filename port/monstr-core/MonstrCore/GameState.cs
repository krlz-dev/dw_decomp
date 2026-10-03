using System;
using System.Collections.Generic;
using System.Linq;

namespace MonstrCore
{
    /// <summary>
    /// Explicit, named world state.
    ///
    /// This is a direct response to what the DW1 decomp measurement showed: that
    /// game kept its entire global state in a flat array of 256 integers, of
    /// which six have been identified after six months of reverse engineering.
    /// PSTAT_104 = 3 tells you nothing. That opacity is exactly why DW1's
    /// progression is hard to reason about today.
    ///
    /// Named keys fix the readability. They introduce a different failure mode
    /// though: a typo silently reads as "false" and the bug surfaces hours later
    /// as "the door never opened". So every key must be REGISTERED, and touching
    /// an unregistered key throws. Loud failure beats a silent default.
    /// </summary>
    public sealed class GameState
    {
        private readonly Dictionary<string, bool> _flags = new();
        private readonly Dictionary<string, int> _counters = new();

        public CreatureState Creatures { get; }
        public WorldState World { get; }
        public SettlementState Settlement { get; }
        public StoryState Story { get; }
        public PartnerState Partner { get; }

        public GameState()
        {
            Creatures = new CreatureState(this);
            World = new WorldState(this);
            Settlement = new SettlementState(this);
            Story = new StoryState(this);
            Partner = new PartnerState(this);

            foreach (var k in StateKeys.AllFlags) _flags[k] = false;
            foreach (var k in StateKeys.AllCounters) _counters[k] = 0;
        }

        public bool GetFlag(string key)
        {
            if (!_flags.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllFlags);
            return _flags[key];
        }

        public void SetFlag(string key, bool value)
        {
            if (!_flags.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllFlags);
            _flags[key] = value;
        }

        public int GetCounter(string key)
        {
            if (!_counters.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllCounters);
            return _counters[key];
        }

        public void SetCounter(string key, int value)
        {
            if (!_counters.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllCounters);
            if (value < 0)
                throw new ArgumentOutOfRangeException(nameof(value),
                    $"'{key}' cannot go negative (got {value}). Progression counters only move forward.");
            _counters[key] = value;
        }

        /// <summary>Flags currently true, sorted. For save files and test diffing.</summary>
        public IEnumerable<string> ActiveFlags() =>
            _flags.Where(kv => kv.Value).Select(kv => kv.Key).OrderBy(k => k);

        public IEnumerable<KeyValuePair<string, int>> NonZeroCounters() =>
            _counters.Where(kv => kv.Value != 0).OrderBy(kv => kv.Key);

        internal IReadOnlyDictionary<string, bool> RawFlags => _flags;
        internal IReadOnlyDictionary<string, int> RawCounters => _counters;
    }

    public sealed class UnknownStateKeyException : Exception
    {
        public UnknownStateKeyException(string key, IReadOnlyCollection<string> known)
            : base(BuildMessage(key, known)) { }

        private static string BuildMessage(string key, IReadOnlyCollection<string> known)
        {
            // Suggest the closest registered key. A typo in a world-graph YAML
            // file is the most likely cause, and "did you mean" turns a
            // half-hour hunt into a five-second fix.
            var near = known
                .Select(k => (k, d: Distance(k.ToLowerInvariant(), key.ToLowerInvariant())))
                .OrderBy(t => t.d)
                .Where(t => t.d <= 4)
                .Take(3)
                .Select(t => t.k)
                .ToList();

            var hint = near.Count > 0
                ? $" Did you mean: {string.Join(", ", near)}?"
                : $" No similar key registered. Add it to StateKeys.";
            return $"Unregistered state key '{key}'.{hint}";
        }

        private static int Distance(string a, string b)
        {
            var d = new int[a.Length + 1, b.Length + 1];
            for (var i = 0; i <= a.Length; i++) d[i, 0] = i;
            for (var j = 0; j <= b.Length; j++) d[0, j] = j;
            for (var i = 1; i <= a.Length; i++)
                for (var j = 1; j <= b.Length; j++)
                    d[i, j] = Math.Min(Math.Min(d[i - 1, j] + 1, d[i, j - 1] + 1),
                                       d[i - 1, j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1));
            return d[a.Length, b.Length];
        }
    }
}
