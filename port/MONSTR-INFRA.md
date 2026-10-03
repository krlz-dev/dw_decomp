# MONSTR Infrastructure Assessment

> **Question:** can we run the Unity/MCP/agent pipeline on a VPS?
> **Short answer:** yes for most of it, with one licensing clause to resolve
> first and one hard limit that no amount of money fixes cheaply.
>
> Everything below is measured or quoted, not estimated. Sources are linked.

---

## 1. The licensing clause to resolve before paying for anything

This is the first thing to settle because it is a terms question, not a
technical one, and it has a cost attached.

From [Unity's Software Terms of Service](https://unity.com/legal/terms-of-service/software),
§2.8 *Concurrent use*:

> "Unless we make available and you have purchased **floating licenses** [...]
> you (and your Authorized Users) may install the Unity Editor on **both a
> primary and a secondary computer** or operating system, solely for your
> convenience, but only for use by a single [user] [...] you may only use
> **one instance at any given time per seat**."

And §2.5:

> "you must purchase a separate **Unity Build Server** subscription in order to
> use Unity Build Server"

### What that means concretely

| Setup | Allowed? |
|---|---|
| Editor on your PC + Editor on one Hetzner box, **used one at a time** | Yes, §2.8 "primary and secondary computer" |
| Both running **simultaneously** (you working while an agent works) | No, needs floating licences |
| Headless `-batchmode` builds on a build server | Needs a Build Server subscription per §2.5 |

The Phase 2 plan in your spec has agents working on the server *while* you work
locally. Read literally, that is two concurrent instances on one seat.

**Action:** confirm with Unity sales before provisioning. The answer changes the
monthly cost, not the architecture. Do not take my reading of a ToS as legal
advice; this is a flag, not a ruling.

---

## 2. What the hardware spec actually has to satisfy

[Unity 6 system requirements](https://docs.unity3d.com/6000.0/Documentation/Manual/system-requirements.html),
Linux, verbatim:

> "Ubuntu 22.04, Ubuntu 24.04 · X64 architecture with SSE2 · OpenGL 3.2+ or
> Vulkan-capable, **Nvidia and AMD GPUs** · **Gnome desktop environment running
> on top of X11 or Wayland** · Nvidia official proprietary graphics driver, or
> AMD Mesa graphics driver"

> "a minimum of **8 GB RAM** is recommended [...] it's recommended to use a disk
> drive with a **high IOPS** rating"

A standard cloud VPS provides a virtio GPU, no vendor driver, no desktop. By the
letter of that spec, the interactive Editor is unsupported there.

### Measured: the software path works anyway

Tested on a plain Hetzner-class VPS with no GPU, using Xvfb and Mesa:

```
OpenGL renderer:       llvmpipe (LLVM 20.1.2, 256 bits)
OpenGL version:        4.5 (Compatibility Profile) Mesa 25.2.8
Max core profile:      4.5
```

**OpenGL 4.5 in pure software**, above Unity's 3.2 floor. The Editor will
launch. "Unsupported" is not "blocked".

### Measured, and then discarded as meaningless

I benchmarked that software renderer and got 700-860 FPS. I am recording the
number here only to explain why it should be ignored:

```
640x480    717 FPS
1280x720   716 FPS
1920x1080  760 FPS
```

Flat across a 9x pixel increase. That means the benchmark was CPU-bound on
geometry submission and never touched fill rate, which is exactly where
llvmpipe collapses. It proves OpenGL functions. It says nothing about Unity's
framerate, and quoting it as if it did would be the same mistake as estimating
a GTE call count with a grep.

**Expect single-digit FPS on a lit 3D scene under llvmpipe.** That is the
limit, and it is why the GPU question is real rather than theoretical.

---

## 3. The split that matters: Editor-attached vs headless

All the mature Unity MCP servers bridge to a **running Editor process**:

| stars | project | description |
|---|---|---|
| 14,669 | `CoplayDev/unity-mcp` | "bridge between AI assistants and your Unity Editor" |
| 4,383 | `IvanMurzak/Unity-MCP` | "full AI develop and test loop" |
| 1,917 | `CoderGamester/mcp-unity` | "connect with Unity Editor" |

So the agent workflow in the mechanics plan §18 needs a live Editor, held open
for hours. That is a different workload from a build server that starts, runs
`-executeMethod`, and exits.

### What runs headless, with no renderer at all

This is the part the architecture got right by accident. Because the region
blueprint §16 keeps the world graph machine-readable and §13/§15 keep state
explicit and named, most agent tasks never touch a pixel:

| Task from blueprint §18 | Needs GPU? |
|---|---|
| Create scene, spawn point, transitions | No, scene graph is data |
| Add encounter marker, set `Creature.Mossu.Recruited` | No |
| Run progression test | No, pure C# |
| Verify DeepGrove reachable, Canyon blocked | No, graph traversal |
| Asset import, compile, build, read console errors | No |

**The entire blueprint §19 vertical slice checklist — all 13 items through
"save, quit/reload, state remains correct" — is a batchmode test suite.**

That is the strongest argument for the VPS, and it comes from the design
decision to keep progression in data rather than hand-authored scene scripts.
Had it gone the other way, none of it would be headless-testable.

### What does not work, at any price short of a GPU

Quoting [Unity's command-line docs](https://docs.unity3d.com/6000.0/Documentation/Manual/EditorCommandLineArguments.html)
on `-nographics`:

> "Unity doesn't initialize the graphics device. You can then run automated
> workflows on machines that don't have a GPU. **Automated workflows only work
> when you have a window in focus**, otherwise you can't send simulated input
> commands. **-nographics does not allow you to bake GI**"

- Play-mode visual verification at usable framerate
- Lighting/GI bakes
- Reliable simulated input
- Any judgement of how the game *looks* or *feels*

---

## 4. Agreed provisioning

The phased plan in the brief is right, and cheaper than my instinct was. Keeping
it, with the licensing caveat attached to Phase 2 onward:

```
Phase 1  prototype          dev PC + GitHub                       ~EUR 0
Phase 2  agents/CI useful   Hetzner Cloud ~8 vCPU / 32 GB         ~EUR 20-50/mo
Phase 3  agents constant    dedicated 12-16 core / 64 GB / 1 TB   ~EUR 100+/mo
Phase 4  visual agent work  add GPU (GEX class) only if proven
```

Rationale worth recording, because it is the part people get wrong:

**64 GB is not for the game.** MONSTR is deliberately low-poly. The RAM is for
concurrency: Editor + MCP + several agents + Git worktrees + tests + Blender +
asset import + language server, all live at once.

**Git worktrees per agent**, so parallel agents do not fight over one scene:

```
/main
/agents/combat
/agents/world
/agents/ui
/agents/tests
```

Each agent isolated, tests its own branch, submits changes.

**Dedicated over shared, eventually.** Unity compilation and asset import are
bursty on CPU and disk. Unity's own docs call out high IOPS specifically for
builds. Shared vCPU makes build times unpredictable.

**Do not rent a GPU 24/7 at the start.** It would idle through the phase where
the work is systems and data, not rendering.

---

## 5. Where I would spend the first euro

Not on infrastructure.

The blueprint §19 slice — Rui, Kiro, forest, Mossu encounter, fight,
negotiation, Mossu moves to town, greenhouse changes, save/reload — is the thing
that tells you whether autonomous Unity agents are productive enough to justify
any of the above.

That slice runs on a dev PC. The honest sequence is: build it locally, measure
how much of it an agent could have done headless, then provision for the answer.

---

## 6. Open items

- **Unity licensing for concurrent use.** Flagged above. Needs Unity sales, not
  a ToS reading by an engineer.
- **GPU VPS pricing.** Hetzner GEX exists and removes every limit in §2 and §3.
  I have not priced it and will not quote a figure I have not verified.
- **llvmpipe Unity Editor framerate.** Measurable, not yet measured. Requires
  installing the Editor, which needs the licensing answer first.
