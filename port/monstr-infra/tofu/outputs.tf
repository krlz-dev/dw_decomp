output "server_ipv4" {
  description = "Public IPv4 of the agent/build machine."
  value       = hcloud_server.agents.ipv4_address
}

output "server_ipv6" {
  value = hcloud_server.agents.ipv6_address
}

output "ssh_command" {
  description = "Connect to the machine."
  value       = "ssh root@${hcloud_server.agents.ipv4_address}"
}

output "mcp_tunnel_command" {
  description = "Forward the Unity MCP port over SSH. The MCP port is never exposed publicly."
  value       = "ssh -N -L 8090:127.0.0.1:8090 root@${hcloud_server.agents.ipv4_address}"
}

output "workspace_device" {
  description = "Block device for the workspace volume, as cloud-init mounts it."
  value       = hcloud_volume.workspace.linux_device
}

output "monthly_cost_reminder" {
  description = "Resources that bill monthly. Confirm actual prices in the Hetzner console; these are resource names, not quotes."
  value = join(", ", [
    "server ${var.server_type}",
    "volume ${var.volume_size_gb}GB",
    var.enable_backups ? "backups +20%" : "no backups",
  ])
}
