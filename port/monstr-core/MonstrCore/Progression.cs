using System;
using System.Collections.Generic;
using System.Linq;

namespace MonstrCore
{
    public interface ICondition
    {
        bool Evaluate(GameState state);
        /// <summary>Human-readable, for test failures and agent diagnostics.</summary>
        string Describe();
    }

    public interface IGameAction
    {
        void Execute(GameState state);
        string Describe();
    }

    // ---- conditions ---------------------------------------------------------

    public sealed class FlagSet : ICondition
    {
        private readonly string _key;
        private readonly bool _expected;

        public FlagSet(string key, bool expected = true)
        {
            _key = key;
            _expected = expected;
        }

        public bool Evaluate(GameState s) => s.GetFlag(_key) == _expected;
        public string Describe() => _expected ? _key : $"NOT {_key}";
    }

    /// <summary>
    /// Threshold comparison with an INVERTIBLE direction.
    ///
    /// Lifted as a pattern (not an implementation) from the one genuinely clever
    /// thing in DW1's evolution code: a flag bit that reverses the comparison,
    /// so some evolutions require care mistakes ABOVE a number rather than
    /// below. One bit, and a neglect path plus a devotion path share a single
    /// data table.
    ///
    /// Worth having from day one, because retrofitting it means rewriting every
    /// requirement row.
    /// </summary>
    public sealed class CounterThreshold : ICondition
    {
        private readonly string _key;
        private readonly int _value;
        private readonly bool _inverted;

        public CounterThreshold(string key, int value, bool inverted = false)
        {
            _key = key;
            _value = value;
            _inverted = inverted;
        }

        public bool Evaluate(GameState s)
        {
            var actual = s.GetCounter(_key);
            return _inverted ? actual <= _value : actual >= _value;
        }

        public string Describe() => $"{_key} {(_inverted ? "<=" : ">=")} {_value}";
    }

    /// <summary>
    /// Band condition. Weight in DW1 is a +-5 window, not a minimum, which is
    /// why overfeeding fails the same check as starving. That is a better
    /// mechanic than a floor and costs nothing extra.
    /// </summary>
    public sealed class CounterInBand : ICondition
    {
        private readonly string _key;
        private readonly int _target;
        private readonly int _tolerance;

        public CounterInBand(string key, int target, int tolerance)
        {
            if (tolerance < 0) throw new ArgumentOutOfRangeException(nameof(tolerance));
            _key = key;
            _target = target;
            _tolerance = tolerance;
        }

        public bool Evaluate(GameState s)
        {
            var v = s.GetCounter(_key);
            return v >= _target - _tolerance && v <= _target + _tolerance;
        }

        public string Describe() => $"{_key} within {_target}+-{_tolerance}";
    }

    public sealed class AllOf : ICondition
    {
        private readonly ICondition[] _parts;
        public AllOf(params ICondition[] parts) => _parts = parts;

        public bool Evaluate(GameState s) => _parts.All(p => p.Evaluate(s));
        public string Describe() => _parts.Length == 0
            ? "(always true)"
            : string.Join(" AND ", _parts.Select(p => p.Describe()));

        /// <summary>Which sub-conditions failed. The useful half of a failed gate.</summary>
        public IEnumerable<string> FailingParts(GameState s) =>
            _parts.Where(p => !p.Evaluate(s)).Select(p => p.Describe());
    }

    public sealed class AnyOf : ICondition
    {
        private readonly ICondition[] _parts;
        public AnyOf(params ICondition[] parts) => _parts = parts;

        public bool Evaluate(GameState s) => _parts.Any(p => p.Evaluate(s));
        public string Describe() => string.Join(" OR ", _parts.Select(p => p.Describe()));
    }

    // ---- actions ------------------------------------------------------------

    public sealed class SetFlag : IGameAction
    {
        private readonly string _key;
        private readonly bool _value;

        public SetFlag(string key, bool value = true)
        {
            _key = key;
            _value = value;
        }

        public void Execute(GameState s) => s.SetFlag(_key, _value);
        public string Describe() => $"set {_key} = {_value}";
    }

    public sealed class SetCounter : IGameAction
    {
        private readonly string _key;
        private readonly int _value;

        public SetCounter(string key, int value)
        {
            _key = key;
            _value = value;
        }

        public void Execute(GameState s) => s.SetCounter(_key, _value);
        public string Describe() => $"set {_key} = {_value}";
    }

    public sealed class AddCounter : IGameAction
    {
        private readonly string _key;
        private readonly int _delta;

        public AddCounter(string key, int delta)
        {
            _key = key;
            _delta = delta;
        }

        public void Execute(GameState s) =>
            s.SetCounter(_key, Math.Max(0, s.GetCounter(_key) + _delta));

        public string Describe() => $"{_key} {(_delta >= 0 ? "+" : "")}{_delta}";
    }

    /// <summary>
    /// A guarded sequence: actions run only if the condition holds, and the
    /// whole batch is atomic.
    ///
    /// Atomicity matters here specifically. A recruitment sets several pieces of
    /// state at once (recruited, lives-in-town, greenhouse unlocked, prosperity
    /// up). Half-applying that produces a monster that is recruited but has no
    /// home, which is the kind of state corruption that survives a save file and
    /// is miserable to diagnose later.
    /// </summary>
    public sealed class GuardedEvent
    {
        public string Id { get; }
        public ICondition Condition { get; }
        private readonly IGameAction[] _actions;

        public GuardedEvent(string id, ICondition condition, params IGameAction[] actions)
        {
            Id = id;
            Condition = condition;
            _actions = actions;
        }

        public bool CanFire(GameState s) => Condition.Evaluate(s);

        public EventResult TryFire(GameState s)
        {
            if (!Condition.Evaluate(s))
            {
                var why = Condition is AllOf all
                    ? string.Join("; ", all.FailingParts(s))
                    : Condition.Describe();
                return EventResult.Blocked(Id, why);
            }

            // Snapshot first. An exception part-way through (an unregistered key,
            // a negative counter) must not leave the world half-changed.
            var backup = SaveGame.Serialise(s);
            try
            {
                foreach (var a in _actions) a.Execute(s);
                return EventResult.Fired(Id, _actions.Select(a => a.Describe()).ToArray());
            }
            catch (Exception ex)
            {
                SaveGame.RestoreInto(s, backup);
                throw new EventExecutionException(Id, ex);
            }
        }
    }

    public sealed record EventResult(string EventId, bool DidFire, string Reason, string[] Applied)
    {
        public static EventResult Fired(string id, string[] applied) =>
            new(id, true, "fired", applied);

        public static EventResult Blocked(string id, string why) =>
            new(id, false, why, Array.Empty<string>());
    }

    public sealed class EventExecutionException : Exception
    {
        public EventExecutionException(string eventId, Exception inner)
            : base($"Event '{eventId}' failed mid-execution and was rolled back: {inner.Message}", inner) { }
    }
}
