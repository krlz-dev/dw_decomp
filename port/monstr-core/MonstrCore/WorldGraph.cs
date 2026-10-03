using System;
using System.Collections.Generic;
using System.Linq;

namespace MonstrCore
{
    /// <summary>
    /// The world as a gated directed graph.
    ///
    /// This is the piece that makes the whole VPS argument work. Because
    /// progression lives in data rather than hand-authored scene scripts, "can
    /// the player reach DeepGrove?" and "is Canyon still blocked?" are graph
    /// traversals. No renderer, no Unity, no GPU.
    /// </summary>
    public sealed class WorldGraph
    {
        private readonly Dictionary<string, Area> _areas = new();

        public WorldGraph Add(Area area)
        {
            if (_areas.ContainsKey(area.Id))
                throw new ArgumentException($"Duplicate area id '{area.Id}'.");
            _areas[area.Id] = area;
            return this;
        }

        public Area this[string id] =>
            _areas.TryGetValue(id, out var a)
                ? a
                : throw new ArgumentException($"No area '{id}'. Known: {string.Join(", ", _areas.Keys.OrderBy(k => k))}");

        public IEnumerable<string> AreaIds => _areas.Keys.OrderBy(k => k);

        /// <summary>Areas reachable from <paramref name="start"/> under current state.</summary>
        public IReadOnlySet<string> Reachable(string start, GameState state)
        {
            if (!_areas.ContainsKey(start))
                throw new ArgumentException($"No area '{start}'.");

            var seen = new HashSet<string> { start };
            var queue = new Queue<string>();
            queue.Enqueue(start);

            while (queue.Count > 0)
            {
                foreach (var exit in _areas[queue.Dequeue()].Exits)
                {
                    if (!exit.IsOpen(state)) continue;
                    if (seen.Add(exit.Target)) queue.Enqueue(exit.Target);
                }
            }
            return seen;
        }

        public bool CanReach(string start, string target, GameState state) =>
            Reachable(start, state).Contains(target);

        /// <summary>
        /// Why a target is unreachable: the gates that would have to open.
        /// Without this, a failing progression test says "false" and the next
        /// half hour goes into finding out which gate.
        /// </summary>
        public IReadOnlyList<string> ExplainUnreachable(string start, string target, GameState state)
        {
            var reached = Reachable(start, state);
            if (reached.Contains(target))
                return Array.Empty<string>();

            var blockers = new List<string>();
            foreach (var id in reached)
                foreach (var exit in _areas[id].Exits)
                    if (!exit.IsOpen(state))
                        blockers.Add($"{id} -> {exit.Target} needs {exit.Requirement!.Describe()}");

            return blockers.Count > 0
                ? blockers
                : new List<string> { $"'{target}' has no inbound path from '{start}' at all (graph gap, not a gate)" };
        }

        /// <summary>
        /// Structural check, independent of game state: every area reachable if
        /// all gates were open. Catches an area wired into the data but with no
        /// path to it, which is a content bug an agent can introduce silently.
        /// </summary>
        public IReadOnlyList<string> FindOrphans(string start)
        {
            var seen = new HashSet<string> { start };
            var queue = new Queue<string>();
            queue.Enqueue(start);
            while (queue.Count > 0)
                foreach (var e in _areas[queue.Dequeue()].Exits)
                    if (seen.Add(e.Target)) queue.Enqueue(e.Target);

            return _areas.Keys.Where(k => !seen.Contains(k)).OrderBy(k => k).ToList();
        }

        /// <summary>Exits pointing at areas that do not exist.</summary>
        public IReadOnlyList<string> FindDanglingExits() =>
            _areas.Values
                  .SelectMany(a => a.Exits.Select(e => (from: a.Id, e.Target)))
                  .Where(t => !_areas.ContainsKey(t.Target))
                  .Select(t => $"{t.from} -> {t.Target} (no such area)")
                  .OrderBy(s => s)
                  .ToList();
    }

    public sealed class Area
    {
        public string Id { get; }
        public string DisplayName { get; }
        private readonly List<Exit> _exits = new();
        public IReadOnlyList<Exit> Exits => _exits;

        public Area(string id, string displayName)
        {
            Id = id;
            DisplayName = displayName;
        }

        public Area Exit(string target, ICondition? requires = null)
        {
            _exits.Add(new Exit(target, requires));
            return this;
        }
    }

    public sealed class Exit
    {
        public string Target { get; }
        public ICondition? Requirement { get; }

        public Exit(string target, ICondition? requirement)
        {
            Target = target;
            Requirement = requirement;
        }

        public bool IsOpen(GameState state) =>
            Requirement is null || Requirement.Evaluate(state);
    }
}
