using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MonstrCore
{
    /// <summary>
    /// Save and load, as plain text.
    ///
    /// Deliberately not a binary blob or a Unity-serialised object. A save file
    /// you can read in a terminal is one an agent can diff, a test can assert
    /// on, and a human can debug. The DW1 memory-card format is the opposite of
    /// this and it is the reason its progression took six months to decode.
    ///
    /// Format: one "key=value" per line, flags as true/false, counters as ints.
    /// Keys are sorted, so two saves of the same state are byte-identical and
    /// `diff` tells you exactly what a step changed.
    /// </summary>
    public static class SaveGame
    {
        public const int FormatVersion = 1;

        public static string Serialise(GameState s)
        {
            var sb = new StringBuilder();
            sb.Append("monstr-save v").Append(FormatVersion).Append('\n');

            foreach (var kv in s.RawFlags.OrderBy(k => k.Key, StringComparer.Ordinal))
                sb.Append(kv.Key).Append('=').Append(kv.Value ? "true" : "false").Append('\n');

            foreach (var kv in s.RawCounters.OrderBy(k => k.Key, StringComparer.Ordinal))
                sb.Append(kv.Key).Append('=').Append(kv.Value).Append('\n');

            return sb.ToString();
        }

        public static GameState Deserialise(string text)
        {
            var state = new GameState();
            RestoreInto(state, text);
            return state;
        }

        public static void RestoreInto(GameState s, string text)
        {
            var lines = text.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length == 0 || !lines[0].StartsWith("monstr-save v"))
                throw new SaveFormatException("Missing or malformed header line.");

            var version = lines[0]["monstr-save v".Length..].Trim();
            if (version != FormatVersion.ToString())
                throw new SaveFormatException(
                    $"Save is format v{version}, this build reads v{FormatVersion}. " +
                    "Migration is not implemented: write one before changing the format.");

            var seen = new HashSet<string>();

            foreach (var line in lines.Skip(1))
            {
                var i = line.IndexOf('=');
                if (i <= 0)
                    throw new SaveFormatException($"Malformed line: '{line}'");

                var key = line[..i];
                var val = line[(i + 1)..];

                if (!seen.Add(key))
                    throw new SaveFormatException($"Duplicate key in save: '{key}'");

                // Unknown keys are a hard error, not a skip. A save written by a
                // newer build silently losing state is how progression bugs get
                // blamed on gameplay code.
                if (StateKeys.AllFlags.Contains(key))
                {
                    s.SetFlag(key, val switch
                    {
                        "true" => true,
                        "false" => false,
                        _ => throw new SaveFormatException($"'{key}' expects true/false, got '{val}'")
                    });
                }
                else if (StateKeys.AllCounters.Contains(key))
                {
                    if (!int.TryParse(val, out var n))
                        throw new SaveFormatException($"'{key}' expects an integer, got '{val}'");
                    s.SetCounter(key, n);
                }
                else
                {
                    throw new SaveFormatException(
                        $"Save contains unregistered key '{key}'. Either it was removed from " +
                        "StateKeys without a migration, or the save came from a different build.");
                }
            }

            var missing = StateKeys.AllFlags.Concat(StateKeys.AllCounters)
                                            .Where(k => !seen.Contains(k))
                                            .ToList();
            if (missing.Count > 0)
                throw new SaveFormatException(
                    $"Save is missing {missing.Count} registered key(s): " +
                    string.Join(", ", missing.Take(5)) +
                    (missing.Count > 5 ? ", ..." : ""));
        }
    }

    public sealed class SaveFormatException : Exception
    {
        public SaveFormatException(string message) : base(message) { }
    }
}
