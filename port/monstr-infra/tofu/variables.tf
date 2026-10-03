variable "server_type" {
  description = <<-EOT
    Hetzner Cloud server type for the agent/build machine.

    IMPORTANT: verify this name before the first apply. Hetzner's type names
    change between generations (cx/cpx are shared vCPU, ccx is dedicated vCPU,
    cax is Arm64) and an invalid name fails at apply time, not at plan time.

      hcloud server-type list

    Sizing rationale is in ../README.md. The short version: Phase 2 wants
    roughly 8 vCPU and 32 GB, and the RAM is for concurrency (Editor + MCP +
    several agents + worktrees + tests + Blender), not for the game itself.

    Prefer a DEDICATED vCPU type (ccx line) once builds matter: Unity
    compilation and asset import are bursty on CPU and disk, and shared vCPU
    makes build times unpredictable. Unity's own docs call out high IOPS
    specifically for build performance.
  EOT
  type        = string
  default     = "ccx33"

  validation {
    condition     = can(regex("^(cx|cpx|ccx|cax)[0-9]{2}$", var.server_type))
    error_message = "Must look like a Hetzner Cloud type: cx22, cpx31, ccx33, cax21. Run 'hcloud server-type list' to confirm the exact name exists."
  }
}

variable "location" {
  description = "Hetzner location. fsn1/nbg1 are Germany, hel1 Finland, ash/hil US, sin Singapore."
  type        = string
  default     = "fsn1"

  validation {
    condition     = contains(["fsn1", "nbg1", "hel1", "ash", "hil", "sin"], var.location)
    error_message = "Unknown location. Run 'hcloud location list'."
  }
}

variable "image" {
  description = "Base image. Unity 6 officially supports Ubuntu 22.04 and 24.04 only."
  type        = string
  default     = "ubuntu-24.04"

  validation {
    condition     = contains(["ubuntu-24.04", "ubuntu-22.04"], var.image)
    error_message = "Unity 6 supports Ubuntu 22.04 and 24.04. Anything else is unsupported by Unity."
  }
}

variable "ssh_public_key_path" {
  description = "Path to the public key that will be authorised on the server."
  type        = string
  default     = "~/.ssh/id_ed25519.pub"
}

variable "admin_ipv4" {
  description = <<-EOT
    Your public IPv4 in CIDR form, e.g. "203.0.113.10/32".

    SSH is restricted to this address. Leaving it as 0.0.0.0/0 exposes SSH to
    the whole internet, which on a machine holding source code and agent
    credentials is not an acceptable default. Find it with:

      curl -s https://ipv4.icanhazip.com
  EOT
  type        = string

  validation {
    condition     = can(cidrhost(var.admin_ipv4, 0)) && var.admin_ipv4 != "0.0.0.0/0"
    error_message = "Must be a valid CIDR and must not be 0.0.0.0/0. Use a /32 for a single address."
  }
}

variable "volume_size_gb" {
  description = <<-EOT
    Separate volume for the Unity Library cache, build output and asset imports.

    Kept off the boot disk on purpose: these directories grow fast and are
    churn-heavy, and a full root filesystem takes the whole machine down. 250 GB
    is the Phase 2 floor; Unity's Library cache alone reaches tens of GB on a
    real project.
  EOT
  type        = number
  default     = 250

  validation {
    condition     = var.volume_size_gb >= 100 && var.volume_size_gb <= 10240
    error_message = "Hetzner volumes are 10 GB to 10 TB; below 100 GB is false economy for a Unity workspace."
  }
}

variable "enable_backups" {
  description = "Hetzner automatic backups (+20% cost). Git is the real safety net for source; this covers the machine's own configuration."
  type        = bool
  default     = false
}

variable "project" {
  description = "Prefix for resource names and label values."
  type        = string
  default     = "monstr"
}
