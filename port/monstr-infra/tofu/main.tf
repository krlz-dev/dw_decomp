terraform {
  required_version = ">= 1.6"

  required_providers {
    hcloud = {
      source = "hetznercloud/hcloud"
      # 1.69.0 published 2026-09-11, verified against the Terraform registry API.
      version = "~> 1.69"
    }
  }

  # Remote state is deliberately NOT configured here. For a single-operator
  # project local state is fine, and a half-configured backend is worse than
  # none. When a second person or a CI runner needs to apply, add an S3-compatible
  # backend block (Hetzner Object Storage works) and migrate with `tofu init
  # -migrate-state`.
}

# Token comes from HCLOUD_TOKEN in the environment. It must never appear in a
# .tf file, a .tfvars file, or the repo. The provider reads it automatically.
provider "hcloud" {}
