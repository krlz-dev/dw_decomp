# MONSTR — Unity Dev Machine Setup

> **For:** whoever owns the Unity workstation
> **Goal:** a machine that can open the Editor, consume `MONSTR.Core`, run the
> headless suite, and drive the Editor from an AI agent over MCP.
> **This is the LOCAL workstation.** Server-side Editor automation is blocked on
> the licence question in §9 — read that before provisioning anything remote.
>
> Everything marked **verified** was checked against the live source while
> writing this. Everything marked **unverified** was not, and says why.

---

## Quick start

Run the readiness check first. It installs nothing and tells you what is
missing:

```bash
bash port/scripts/check-dev-machine.sh
```

It checks the OS, cores, RAM, disk, **whether your GPU is real or a software
rasteriser**, the Editor's runtime libraries, the toolchain, kernel file-watcher
limits, and then actually runs the 42-test core suite. Exit code is the number
of blocking failures.

Sample output from a machine that is deliberately not ready:

```
--- GPU (the one that decides whether the Editor is usable) ---
  warn  glxinfo gave no renderer — are you in a graphical session?

--- toolchain ---
  ok    .NET SDK 8.0.425
  ok    uv 0.12.1
  MISS  git-lfs missing — Unity projects carry binary assets

--- the core itself ---
  ok    headless suite green (Failed: 0, Passed: 42)

ready: 7   missing: 2   warnings: 6
```

Then work through the sections below for whatever it flagged.

---

## 0. Before you install anything

Settle this first, because it changes what you are allowed to run.

From [Unity's Software ToS](https://unity.com/legal/terms-of-service/software)
§2.8 (**verified**, quoted directly):

> "Unless we make available and you have purchased **floating licenses** [...]
> you (and your Authorized Users) may install the Unity Editor on **both a
> primary and a secondary computer** or operating system, solely for your
> convenience, but only for use by a single [user] [...] you may only use
> **one instance at any given time per seat**."

And §2.5:

> "you must purchase a separate **Unity Build Server** subscription in order to
> use Unity Build Server"

**What that permits, as I read it:** the Editor on your laptop *and* one other
machine, used one at a time. **What it does not obviously permit:** an agent
driving the Editor on a server while you work locally. That is two concurrent
instances on one seat.

I am an engineer quoting a contract, not a lawyer. **Ask Unity before you put a
remote Editor in CI.** This setup guide covers the local machine only, which is
unambiguous.

---

## 1. Hardware

| | Minimum | Comfortable |
|---|---|---|
| CPU | 8 fast cores | 12–16 |
| RAM | 32 GB | 64 GB |
| Disk | 250 GB NVMe | 1 TB NVMe |
| GPU | Nvidia or AMD, OpenGL 3.2+/Vulkan | any current mid-range |
| OS | Ubuntu 24.04 | Ubuntu 24.04 |

Unity's stated requirement (**verified**, from the
[system requirements page](https://docs.unity3d.com/6000.0/Documentation/Manual/system-requirements.html)):

> "Linux Ubuntu 22.04, Ubuntu 24.04 · X64 with SSE2 · **OpenGL 3.2+ or
> Vulkan-capable, Nvidia and AMD GPUs** · **Gnome desktop environment running on
> top of X11 or Wayland** · Nvidia official proprietary graphics driver, or AMD
> Mesa graphics driver"

> "a minimum of **8 GB RAM** is recommended [...] it's recommended to use a disk
> drive with a **high IOPS** rating"

**Why 32 GB and not 8.** The 8 GB figure is the Editor alone. This machine runs
the Editor *plus* MCP, several agent processes, Git worktrees, the test suite,
Blender and a language server at the same time. The RAM is for concurrency, not
for MONSTR, which is deliberately low-poly.

**On Wayland:** Ubuntu 24.04 supports it with AMD cards, and with Nvidia only on
proprietary driver **550 or above** (**verified**, same page). If you are on
Nvidia and anything looks wrong, log into an X11 session before debugging
anything else.

**Intel integrated graphics is not on Unity's supported list.** It often works.
It is not supported, and "works until it doesn't" is a bad place to be when a
build breaks.

---

## 2. Base system

```bash
sudo apt update && sudo apt upgrade -y

# Editor runtime deps. Finding these out later via a cryptic launch failure
# wastes an afternoon.
sudo apt install -y \
  libgtk-3-0 libnss3 libasound2t64 libgbm1 libxss1 \
  libgconf-2-4 libcanberra-gtk-module

# Build and dev tooling
sudo apt install -y git git-lfs build-essential curl jq ripgrep tmux

# Headless rendering, for running tests without a display
sudo apt install -y xvfb mesa-utils

git lfs install
```

`git-lfs` matters: Unity projects carry binary assets, and committing a 200 MB
FBX to plain Git is a mistake you pay for forever.

### File watcher limits

Unity's asset importer opens a lot of files, and parallel agents multiply it.
Without this you get silent import failures that look like corrupted assets.

```bash
sudo tee /etc/sysctl.d/99-monstr.conf >/dev/null <<'EOF'
fs.inotify.max_user_watches=524288
fs.inotify.max_user_instances=512
fs.file-max=2097152
EOF
sudo sysctl --system
```

---

## 3. Unity Hub and Editor

Hub is distributed for Linux as an **AppImage** and via an **apt repository**.
The apt repo responds (`HTTP 200` on the `Release` file, **verified**) but the
GPG key URL I tried returned **404**, so I cannot give you a verified
`apt-key`/`signed-by` line.

**Use the AppImage.** Fewer moving parts, no key to get wrong:

```bash
mkdir -p ~/Applications
cd ~/Applications
# Get the current link from https://unity.com/download (the versioned filename
# changes; I am not quoting a URL I have not fetched today).
chmod +x UnityHub.AppImage
./UnityHub.AppImage
```

AppImages need FUSE. On 24.04:

```bash
sudo apt install -y libfuse2t64
```

### Editor version

Install a **Unity 6 LTS** release. Current releases follow the `6000.x.yfz`
scheme — `6000.6.4f1` was the latest when this was written (**verified** from
the release archive).

Pin one version and write it into the project. A team drifting across Editor
versions on the same project produces merge conflicts in `.meta` files that
nobody enjoys.

Modules to tick during install:
- **Linux Build Support (IL2CPP)** — needed to produce a playable build for the
  §32 handoff test
- **Documentation** — optional, large
- Skip Android/iOS/WebGL for now. Out of scope per handoff §27.

---

## 4. .NET SDK

`MONSTR.Core` is plain .NET 8 and its test suite does **not** need Unity. That
separation is the whole reason CI is cheap, so keep the SDK installed
independently of the Editor's bundled Mono.

```bash
curl -sSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
bash /tmp/dotnet-install.sh --channel 8.0 --install-dir /opt/dotnet --no-path

sudo tee /etc/profile.d/dotnet.sh >/dev/null <<'EOF'
export DOTNET_ROOT=/opt/dotnet
export PATH="$PATH:/opt/dotnet"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
EOF
source /etc/profile.d/dotnet.sh
dotnet --version     # expect 8.0.x
```

**Verified:** this exact sequence installed `8.0.425` on Ubuntu 24.04 while
writing this guide.

### Prove the core works before touching Unity

```bash
git clone <monstr-repo> ~/monstr
cd ~/monstr/port/monstr-core
dotnet test
```

Expect:

```
Passed!  Failed: 0, Passed: 42, Skipped: 0, Total: 42, Duration: ~100 ms
```

And the guard proof:

```bash
bash scripts/prove-guards.sh
```

Expect six deliberate breakages each turning tests red, then a clean restore.
**If that does not reproduce, stop and fix it before opening Unity.** A green
core is the one thing the spike is allowed to assume.

---

## 5. Unity MCP

**Verified** from the [CoplayDev/unity-mcp](https://github.com/CoplayDev/unity-mcp)
README (14.6k stars, the most used of the three):

> "**Requirements:** Unity **2021.3 LTS → 6.x** · Python **3.10+** (via `uv`)"

So:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv --version
```

Then inside the Unity project:

1. `Window → Package Manager → + → Add package from git URL`
2. ```
   https://github.com/CoplayDev/unity-mcp.git?path=/MCPForUnity#main
   ```
   Pin a tag rather than `#main` for reproducibility — the README offers
   `#v10.0.0` as the then-current release. **Check for a newer tag** rather than
   trusting that number from this document.
3. `Window → MCP for Unity → Configure All Detected Clients`

### Security note, and it is not optional

**MCP is an unauthenticated control channel into the Editor.** It can create,
modify and delete assets. On a local machine bound to loopback that is fine.

Do not expose that port on a network interface. If you ever reach it from
another machine, tunnel it:

```bash
ssh -N -L 8090:127.0.0.1:8090 user@host
```

The Hetzner firewall in `port/monstr-infra/` deliberately has no rule for it.

---

## 6. Blender

Headless asset processing, scriptable with `blender -b -P script.py`. Useful
because the roster groups monsters into shared skeleton families, so batch
operations are worth automating.

```bash
sudo apt install -y blender
blender -b --version
```

---

## 7. Git worktrees for parallel agents

So concurrent agents do not fight over the same Unity scene or asset database.
Each works a branch, tests it, submits changes.

```bash
cd ~/monstr
git worktree add ../monstr-combat -b agents/combat
git worktree add ../monstr-world  -b agents/world
git worktree add ../monstr-ui     -b agents/ui
git worktree add ../monstr-tests  -b agents/tests
git worktree list
```

**Unverified and worth testing early:** whether multiple Unity Editor instances
can hold sibling worktrees of the same project open simultaneously. Unity takes
a lock per project folder, so separate folders *should* be fine, but each
instance also wants its own `Library/` cache — which means disk use multiplies
and the first import on each worktree is slow. And per §9, concurrent Editors
touch the licence question even on one machine.

The safe pattern until that is settled: **agents work the worktrees headlessly;
one Editor instance open at a time, on whichever branch you are playing.**

---

## 8. Verify the whole machine

```bash
# GPU is real, not software
glxinfo -B | grep -E "OpenGL renderer|OpenGL core profile version"
```

If that says `llvmpipe`, you are on the software rasteriser and the Editor will
be unusably slow. Fix the driver before anything else.

```bash
# toolchain
dotnet --version        # 8.0.x
uv --version
blender -b --version
git lfs version

# the core
cd ~/monstr/port/monstr-core && dotnet test

# headless rendering works (for CI-style runs)
xvfb-run -a glxinfo -B | grep "OpenGL renderer"
```

---

## 9. What this machine may NOT do yet

Per handoff §26, and this is a hard stop rather than a caution:

- **Do not install a second concurrent Editor on a server** until Unity confirms
  the licence model. The Ansible role in `port/monstr-infra/` refuses to install
  the Editor without `unity_licence_confirmed=true`, deliberately.
- **Do not make remote Editor operation part of required CI** before that
  answer.
- Headless `.NET` tests can run anywhere today. They need no Editor and no
  licence.

---

## 10. First task on this machine

Not scene building. Handoff §7, **Gate A**:

```
Unity → MONSTR.Core → GameState → Save → Reload → Unity reads restored state
```

A disposable debug scene with buttons:

```
[Defeat Mossu] [Befriend Mossu] [Invite Mossu] [Mossu Moves In] [Save] [Reload]
```

and a text readout:

```
Mossu stage:        Stranger / DefeatedNotFriend / FriendElsewhere /
                    InvitedNotMoved / Resident
Greenhouse level:   N
Prosperity:         N
Kiro battles:       N
```

Gate A passes when the core compiles from Unity, the 42 headless tests still
pass, Unity mutates state only through the domain operations, save/reload works
from Unity, and **no parallel Unity-only progression model has appeared**.

That last one is the real risk. The fastest way to break this architecture is a
`MonoBehaviour` with its own `bool mossuRecruited` field.

---

## Unverified items, collected

Being explicit so nobody trusts the wrong line:

| Item | Why unverified |
|---|---|
| Unity Hub AppImage download URL | versioned filename changes; fetch from unity.com |
| Hub apt GPG key path | the URL I tried returned 404 |
| `libfuse2t64` package name on 24.04 | not installed and tested here |
| MCP package tag `#v10.0.0` | current at README write time; check for newer |
| Multiple Editors on sibling worktrees | needs testing on real hardware |
| Exact Editor install size | Unity does not publish it; budget generously |
