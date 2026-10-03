# MONSTR infrastructure

OpenTofu + cloud-init + Ansible for the MONSTR agent/build machine on Hetzner
Cloud.

```
OpenTofu     what infrastructure exists
cloud-init   bootstraps a brand-new machine, once
Ansible      what software and configuration should be present, repeatably
hcloud CLI   inspection and manual operations only, never the source of truth
```

## Status

The HCL here is **statically validated, not applied**. `tofu plan` has never
run against it because that needs a real `HCLOUD_TOKEN`. What has been checked:

```
cloud-init YAML parses            17 packages, 3 files, 10 commands
HCL brace balance                 5 files
variables declared vs referenced  8 / 8
resource references resolve       7 resources, no dangling refs
no literal tokens committed
cloud-init path resolves
safety guards present             prevent_destroy, firewall-at-creation,
                                  no world-open SSH, password auth off
```

Run that yourself with `bash scripts/validate.sh`.

**Before the first apply, verify the server type exists:**

```bash
hcloud server-type list
```

`server_type` defaults to `ccx33` and that name is **unverified**. Hetzner's
type names change between generations and an invalid one fails at apply time,
not at plan time. The variable has a format validation, which cannot confirm
the type actually exists.

## Usage

```bash
export HCLOUD_TOKEN="..."          # never in a .tf or .tfvars file

cd tofu
tofu init
tofu plan  -var "admin_ipv4=$(curl -s https://ipv4.icanhazip.com)/32"
tofu apply -var "admin_ipv4=$(curl -s https://ipv4.icanhazip.com)/32"
```

Then finish the two deliberate manual steps in `/root/FIRST-RUN.md` on the
new machine, and run Ansible:

```bash
cd ../ansible
ansible-playbook -i inventory.ini playbook.yml
```

## What this provisions

| Resource | Why |
|---|---|
| `hcloud_server` | the agent/build machine, Ubuntu 24.04 |
| `hcloud_firewall` | SSH from one address, ICMP, nothing else |
| `hcloud_volume` 250 GB | Unity Library cache, builds, asset imports |
| `hcloud_network` + subnet | so adding a GPU box later needs no re-addressing |
| `hcloud_ssh_key` | key-only access |

## Decisions worth knowing

**The MCP port is not in the firewall.** Unity MCP is an unauthenticated
control channel into the Editor. Reach it over an SSH tunnel:

```bash
ssh -N -L 8090:127.0.0.1:8090 root@<ip>
```

**The workspace volume is a separate device with `prevent_destroy`.** Unity's
Library cache and build output are large and churn-heavy. Keeping them off the
boot disk means a runaway build cannot fill root and take the machine down, and
`prevent_destroy` means no plan quietly deletes hours of asset import.

**The volume mount uses `nofail`.** If the volume is absent the machine still
boots and stays reachable for repair, instead of dropping to emergency mode.
The tradeoff: a build could start with no workspace mounted, so check
`findmnt /mnt/workspace` in CI before building.

**The device ID in the mount unit is a placeholder on purpose.** It is only
knowable after attachment. Guessing it produces a machine that silently boots
without a workspace, which is worse than one obvious manual step.

**cloud-init installs almost nothing.** It runs once and is painful to iterate
on. Unity, .NET, Blender and agent CLIs belong in Ansible, which is re-runnable.

**8 GB swap.** Unity linking and asset import spike above RAM briefly. An OOM
kill mid-build costs more than a slow build.

**Dedicated vCPU (`ccx`) over shared (`cx`/`cpx`) once builds matter.** Unity
compilation and asset import are bursty on CPU and disk; shared vCPU makes build
times unpredictable. Unity's docs call out high IOPS specifically for builds.

**No remote state backend.** For a single operator, local state is fine and a
half-configured backend is worse than none. When CI or a second person needs to
apply, add an S3-compatible backend (Hetzner Object Storage works) and
`tofu init -migrate-state`.

## Cloud is not Robot

This tree manages **Hetzner Cloud** only. If MONSTR later moves to dedicated
hardware (AX42 class) or a GPU server (GEX class), those live behind the Robot
API, which is a different provisioning surface. `hcloud_server` resources do not
map onto them.

Design that layer separately when the time comes. Do not try to generalise this
one to cover both.

## Unresolved: the Unity licence

From [Unity's Software ToS](https://unity.com/legal/terms-of-service/software),
§2.8:

> "you (and your Authorized Users) may install the Unity Editor on both a
> primary and a secondary computer [...] but only for use by a single [user]
> [...] you may only use **one instance at any given time per seat**."

And §2.5:

> "you must purchase a separate **Unity Build Server** subscription in order to
> use Unity Build Server"

An agent driving the Editor on this machine while you work locally is two
concurrent instances on one seat. **Confirm with Unity before provisioning** —
it changes the monthly cost, not the architecture. That is a terms question, not
an engineering one, and the quotes above are a flag rather than a ruling.
