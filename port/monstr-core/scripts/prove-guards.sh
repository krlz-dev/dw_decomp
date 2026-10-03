#!/bin/bash
# Prove the critical guards fail in RED when their protection is removed.
# A test that passes whether or not the code is correct is worse than no test.
cd /root/repositories/dw_decomp/port/monstr-core || exit 1
export DOTNET_ROOT=/opt/dotnet PATH="$PATH:/opt/dotnet" DOTNET_CLI_TELEMETRY_OPTOUT=1

run() { timeout 400 dotnet test 2>&1 | grep -oE "Failed: +[0-9]+, Passed: +[0-9]+" | head -1; }

echo "=== baseline ==="
run

cp MonstrCore/Progression.cs /tmp/prog.bak
cp MonstrCore/GameState.cs   /tmp/gs.bak
cp MonstrCore/MossuSlice.cs  /tmp/slice.bak

echo ""
echo "=== GUARD 1: remove the atomic rollback from GuardedEvent ==="
python3 - <<'PY'
p='MonstrCore/Progression.cs'
s=open(p).read()
s=s.replace("SaveGame.RestoreInto(s, backup);","/* rollback removed */")
open(p,'w').write(s)
PY
run
cp /tmp/prog.bak MonstrCore/Progression.cs

echo ""
echo "=== GUARD 2: let unregistered state keys through silently ==="
python3 - <<'PY'
p='MonstrCore/GameState.cs'
s=open(p).read()
s=s.replace("""            if (!_flags.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllFlags);
            return _flags[key];""",
"""            return _flags.TryGetValue(key, out var v) && v;""")
open(p,'w').write(s)
PY
run
cp /tmp/gs.bak MonstrCore/GameState.cs

echo ""
echo "=== GUARD 3: collapse defeat and recruit into one step ==="
python3 - <<'PY'
p='MonstrCore/MossuSlice.cs'
s=open(p).read()
s=s.replace("""                new SetFlag(StateKeys.MossuDefeated),
                new AddCounter(StateKeys.KiroBattles, 1)),""",
"""                new SetFlag(StateKeys.MossuDefeated),
                new SetFlag(StateKeys.MossuRecruited),
                new AddCounter(StateKeys.KiroBattles, 1)),""")
open(p,'w').write(s)
PY
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== GUARD 4: make the ruins exit ungated ==="
python3 - <<'PY'
p='MonstrCore/MossuSlice.cs'
s=open(p).read()
s=s.replace("""                .Exit(OvergrownRuins,
                    // The player must notice the damaged vegetation before the
                    // ruins mean anything. Gating on a discovery rather than a
                    // level is blueprint rule 3.
                    new FlagSet(StateKeys.ForestVegetationDamageFound))""",
"""                .Exit(OvergrownRuins)""")
open(p,'w').write(s)
PY
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== restored ==="
run
rm -f /tmp/prog.bak /tmp/gs.bak /tmp/slice.bak
