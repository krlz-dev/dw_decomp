locals {
  labels = {
    project = var.project
    role    = "agent-build"
    managed = "opentofu"
  }
}

resource "hcloud_ssh_key" "monstr" {
  name       = "${var.project}-dev"
  public_key = file(pathexpand(var.ssh_public_key_path))
  labels     = local.labels
}

# Private network. Nothing needs it today with a single server, but adding a
# second machine later (a GPU box for Editor work, per Phase 4) should not
# require re-addressing the first one.
resource "hcloud_network" "monstr" {
  name     = "${var.project}-net"
  ip_range = "10.10.0.0/16"
  labels   = local.labels
}

resource "hcloud_network_subnet" "monstr" {
  network_id   = hcloud_network.monstr.id
  type         = "cloud"
  network_zone = "eu-central"
  ip_range     = "10.10.1.0/24"
}

resource "hcloud_server" "agents" {
  name        = "${var.project}-agents"
  image       = var.image
  server_type = var.server_type
  location    = var.location
  backups     = var.enable_backups
  ssh_keys    = [hcloud_ssh_key.monstr.id]
  labels      = local.labels

  # Firewall is attached at creation. Attaching it afterwards leaves a window
  # where the server is reachable on every port.
  firewall_ids = [hcloud_firewall.agents.id]

  user_data = file("${path.module}/../cloud-init/bootstrap.yaml")

  network {
    network_id = hcloud_network.monstr.id
    ip         = "10.10.1.10"
  }

  public_net {
    ipv4_enabled = true
    ipv6_enabled = true
  }

  depends_on = [hcloud_network_subnet.monstr]

  lifecycle {
    # server_type changes rebuild the machine. Resizing should be a deliberate
    # act with the workspace volume detached, not a side effect of a plan.
    ignore_changes = [user_data]
  }
}

# Workspace volume: Unity Library cache, build output, asset imports.
resource "hcloud_volume" "workspace" {
  name      = "${var.project}-workspace"
  size      = var.volume_size_gb
  location  = var.location
  format    = "ext4"
  labels    = local.labels

  lifecycle {
    # This holds hours of asset import and build cache. Never let a plan
    # destroy it silently.
    prevent_destroy = true
  }
}

resource "hcloud_volume_attachment" "workspace" {
  volume_id = hcloud_volume.workspace.id
  server_id = hcloud_server.agents.id
  automount = false # mounted by cloud-init with explicit fstab options
}
