resource "hcloud_firewall" "agents" {
  name   = "${var.project}-agents-fw"
  labels = local.labels

  # SSH, restricted to one address. This is the only inbound rule by default.
  rule {
    direction  = "in"
    protocol   = "tcp"
    port       = "22"
    source_ips = [var.admin_ipv4]
    description = "SSH from the operator only"
  }

  # ICMP, so the machine is diagnosable.
  rule {
    direction  = "in"
    protocol   = "icmp"
    source_ips = [var.admin_ipv4]
    description = "ping from the operator"
  }

  # Deliberately NOT opened:
  #
  #   Unity MCP port    reach it over an SSH tunnel, never the public internet.
  #                     It is an unauthenticated control channel into the Editor.
  #   VNC / RDP         same reasoning. Tunnel it.
  #   Unity licensing   outbound only, no inbound rule needed.
  #
  # To use MCP from a local machine:
  #   ssh -N -L 8090:127.0.0.1:8090 root@<server-ip>
  #
  # Hetzner firewalls default-deny inbound on anything not listed, and allow all
  # outbound. Outbound is left open because the machine needs apt, GitHub, the
  # Unity Hub and licensing endpoints; narrowing it is a later hardening step,
  # not a first-day one.
}
