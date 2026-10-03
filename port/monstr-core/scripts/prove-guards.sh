#!/bin/bash
# Prove the guards still fail in RED after the terminology change.
# Handoff section 4 makes this an explicit acceptance criterion: the existing
# RED proof that detects collapsing defeat into friendship must remain
# effective after renaming.
cd /root/repositories/dw_decomp/port/monstr-core || exit 1
export DOTNET_ROOT=/opt/dotnet PATH="$PATH:/opt/dotnet" DOTNET_CLI_TELEMETRY_OPTOUT=1

run() { timeout 400 dotnet test 2>&1 | grep -oE "Failed: +[0-9]+, Passed: +[0-9]+" | head -1; }

echo "=== baseline ==="
run

cp MonstrCore/Progression.cs /tmp/prog.bak
cp MonstrCore/GameState.cs   /tmp/gs.bak
cp MonstrCore/MossuSlice.cs  /tmp/slice.bak
cp MonstrCore/StateKeys.cs   /tmp/keys.bak

echo ""
echo "=== GUARD 1: remove the atomic rollback ==="
python3 -c "
p='MonstrCore/Progression.cs'; s=open(p).read()
s=s.replace('SaveGame.RestoreInto(s, backup);','/* removed */')
open(p,'w').write(s)"
run
cp /tmp/prog.bak MonstrCore/Progression.cs

echo ""
echo "=== GUARD 2: let unregistered keys through silently ==="
python3 -c "
p='MonstrCore/GameState.cs'; s=open(p).read()
s=s.replace('''            if (!_flags.ContainsKey(key))
                throw new UnknownStateKeyException(key, StateKeys.AllFlags);
            return _flags[key];''','''            return _flags.TryGetValue(key, out var v) && v;''')
open(p,'w').write(s)"
run
cp /tmp/gs.bak MonstrCore/GameState.cs

echo ""
echo "=== GUARD 3: collapse DEFEAT into BEFRIEND (the constitutional rule) ==="
python3 -c "
p='MonstrCore/MossuSlice.cs'; s=open(p).read()
s=s.replace('''                new SetFlag(StateKeys.MossuDefeated),
                new AddCounter(StateKeys.KiroBattles, 1)),''','''                new SetFlag(StateKeys.MossuDefeated),
                new SetFlag(StateKeys.MossuBefriended),
                new AddCounter(StateKeys.KiroBattles, 1)),''')
open(p,'w').write(s)"
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== GUARD 4: collapse BEFRIEND into LIVES-IN-TOWN ==="
python3 -c "
p='MonstrCore/MossuSlice.cs'; s=open(p).read()
s=s.replace('''                new SetFlag(StateKeys.MossuBefriended),
                new AddCounter(StateKeys.KiroHappiness, 10)),''','''                new SetFlag(StateKeys.MossuBefriended),
                new SetFlag(StateKeys.MossuInvitedToTown),
                new SetFlag(StateKeys.MossuLivesInTown),
                new AddCounter(StateKeys.KiroHappiness, 10)),''')
open(p,'w').write(s)"
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== GUARD 5: ungate the ruins exit ==="
python3 -c "
p='MonstrCore/MossuSlice.cs'; s=open(p).read()
import re
s=re.sub(r'\.Exit\(OvergrownRuins,\s*\n(\s*//[^\n]*\n)+\s*new FlagSet\(StateKeys\.ForestVegetationDamageFound\)\)',
         '.Exit(OvergrownRuins)', s)
open(p,'w').write(s)"
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== GUARD 6: let the grove open on DEFEAT instead of friendship ==="
python3 -c "
p='MonstrCore/MossuSlice.cs'; s=open(p).read()
s=s.replace('new FlagSet(StateKeys.MossuBefriended)));','new FlagSet(StateKeys.MossuDefeated)));')
open(p,'w').write(s)"
run
cp /tmp/slice.bak MonstrCore/MossuSlice.cs

echo ""
echo "=== restored ==="
run
rm -f /tmp/prog.bak /tmp/gs.bak /tmp/slice.bak /tmp/keys.bak
