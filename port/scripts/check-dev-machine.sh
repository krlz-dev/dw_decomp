#!/bin/bash
# MONSTR dev machine readiness check.
#
# Run this on the Unity workstation. It reports what is ready and what is
# missing, and it does NOT install anything: a setup script that silently
# installs things on a developer's machine is worse than a list of failures.
#
#   bash port/scripts/check-dev-machine.sh

pass=0; fail=0; warn=0
ok()   { printf "  \033[32mok\033[0m    %s\n" "$1"; pass=$((pass+1)); }
bad()  { printf "  \033[31mMISS\033[0m  %s\n" "$1"; fail=$((fail+1)); }
note() { printf "  \033[33mwarn\033[0m  %s\n" "$1"; warn=$((warn+1)); }

echo ""
echo "MONSTR dev machine check"
echo "========================"

echo ""
echo "--- OS ---"
if [ -f /etc/os-release ]; then
  . /etc/os-release
  case "$VERSION_ID" in
    24.04|22.04) ok "$PRETTY_NAME (Unity-supported)" ;;
    *)           note "$PRETTY_NAME — Unity 6 supports Ubuntu 22.04 and 24.04 only" ;;
  esac
else
  note "cannot identify the distribution"
fi

echo ""
echo "--- hardware ---"
cores=$(nproc)
[ "$cores" -ge 8 ] && ok "$cores CPU cores" || note "$cores cores (8+ recommended)"

ram=$(free -g | awk '/^Mem:/{print $2}')
if   [ "$ram" -ge 32 ]; then ok "${ram} GB RAM"
elif [ "$ram" -ge 16 ]; then note "${ram} GB RAM — fine for the Editor alone, tight with agents + MCP (32 recommended)"
elif [ "$ram" -ge 8 ];  then note "${ram} GB RAM — at Unity's stated minimum; expect pain with agents running"
else                         bad "${ram} GB RAM — below Unity's 8 GB minimum"
fi

avail=$(df -BG --output=avail / 2>/dev/null | tail -1 | tr -dc '0-9')
[ "${avail:-0}" -ge 100 ] && ok "${avail} GB free on /" \
  || note "${avail:-?} GB free on / — Editor + Library cache + builds grow fast"

echo ""
echo "--- GPU (the one that decides whether the Editor is usable) ---"
if command -v glxinfo >/dev/null 2>&1; then
  r=$(glxinfo -B 2>/dev/null | grep -i "OpenGL renderer" | cut -d: -f2- | xargs)
  v=$(glxinfo -B 2>/dev/null | grep -i "core profile version" | grep -oE "[0-9]+\.[0-9]+" | head -1)
  if echo "$r" | grep -qi "llvmpipe\|softpipe\|swrast"; then
    bad "software rasteriser ($r) — the Editor will be unusably slow, fix the driver"
  elif [ -n "$r" ]; then
    ok "GPU: $r"
    awk -v v="${v:-0}" 'BEGIN{exit !(v+0 >= 3.2)}' \
      && ok "OpenGL core profile $v (Unity needs 3.2+)" \
      || bad "OpenGL core profile ${v:-unknown} — below Unity's 3.2 minimum"
  else
    note "glxinfo gave no renderer — are you in a graphical session?"
  fi
else
  note "glxinfo not installed (apt install mesa-utils) — cannot check the GPU"
fi

if [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
  if lspci 2>/dev/null | grep -qi nvidia; then
    drv=$(modinfo nvidia 2>/dev/null | awk '/^version:/{print $2}' | cut -d. -f1)
    [ -n "$drv" ] && [ "$drv" -ge 550 ] 2>/dev/null \
      && ok "Wayland + Nvidia driver $drv (550+ required)" \
      || note "Wayland + Nvidia driver ${drv:-unknown} — Unity needs 550+; try an X11 session"
  else
    ok "Wayland session (supported with AMD)"
  fi
fi

echo ""
echo "--- Editor runtime deps ---"
missing=""
for lib in libgtk-3-0 libnss3 libgbm1 libxss1; do
  dpkg -s "$lib" >/dev/null 2>&1 || missing="$missing $lib"
done
# the asound package is named differently across releases
dpkg -s libasound2t64 >/dev/null 2>&1 || dpkg -s libasound2 >/dev/null 2>&1 \
  || missing="$missing libasound2t64"
[ -z "$missing" ] && ok "Unity Editor libraries present" \
  || bad "missing:$missing  (sudo apt install$missing)"

echo ""
echo "--- toolchain ---"
if command -v dotnet >/dev/null 2>&1 || [ -x /opt/dotnet/dotnet ]; then
  d=$(command -v dotnet >/dev/null 2>&1 && dotnet --version 2>/dev/null || /opt/dotnet/dotnet --version 2>/dev/null)
  case "$d" in 8.*) ok ".NET SDK $d" ;; *) note ".NET SDK $d — the core targets net8.0" ;; esac
else
  bad ".NET SDK missing — the headless core and its 42 tests need it"
fi

command -v uv >/dev/null 2>&1 && ok "uv $(uv --version 2>/dev/null | awk '{print $2}')" \
  || bad "uv missing — Unity MCP requires it (Python 3.10+ via uv)"

command -v git >/dev/null 2>&1 && ok "git $(git --version | awk '{print $3}')" || bad "git missing"
git lfs version >/dev/null 2>&1 && ok "git-lfs installed" \
  || bad "git-lfs missing — Unity projects carry binary assets"

command -v blender >/dev/null 2>&1 && ok "blender present" \
  || note "blender missing (optional until asset work starts)"

ls ~/Applications/UnityHub.AppImage >/dev/null 2>&1 && ok "Unity Hub AppImage found" \
  || note "Unity Hub not at ~/Applications/UnityHub.AppImage — fine if installed elsewhere"

echo ""
echo "--- kernel limits (Unity's importer opens a lot of files) ---"
w=$(cat /proc/sys/fs/inotify/max_user_watches 2>/dev/null || echo 0)
[ "$w" -ge 524288 ] && ok "inotify watches $w" \
  || note "inotify watches $w — raise to 524288 or imports fail silently"

echo ""
echo "--- the core itself ---"
root="$(cd "$(dirname "$0")/../.." 2>/dev/null && pwd)"
core="$root/port/monstr-core"
if [ -d "$core" ]; then
  ok "found $core"
  if command -v dotnet >/dev/null 2>&1 || [ -x /opt/dotnet/dotnet ]; then
    dn=$(command -v dotnet 2>/dev/null || echo /opt/dotnet/dotnet)
    echo "        running the suite (this is the gate, not a formality)..."
    out=$(cd "$core" && DOTNET_CLI_TELEMETRY_OPTOUT=1 timeout 300 "$dn" test 2>&1 | grep -oE "Failed: +[0-9]+, Passed: +[0-9]+" | head -1)
    if echo "$out" | grep -q "Failed: *0,"; then ok "headless suite green ($out)"
    else bad "headless suite NOT green ($out) — fix this before opening Unity"; fi
  fi
else
  note "monstr-core not found relative to this script"
fi

echo ""
echo "========================"
printf "ready: %d   missing: %d   warnings: %d\n" "$pass" "$fail" "$warn"
echo ""
if [ "$fail" -eq 0 ]; then
  echo "Machine is ready for handoff Gate A:"
  echo "  Unity -> MONSTR.Core -> GameState -> Save -> Reload -> Unity reads it back"
  echo ""
  echo "REMINDER: the Unity licence question (ToS 2.8, concurrent instances per"
  echo "seat) is unresolved. This machine is fine. A remote Editor is not, yet."
else
  echo "Resolve the MISS items above first."
fi
exit "$fail"
