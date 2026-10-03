#!/bin/bash
# Validate the whole infra tree: tofu HCL, cloud-init, and Ansible YAML.
cd "$(dirname "$0")/.." || exit 1
fail=0

echo "=== YAML files parse ==="
for f in cloud-init/bootstrap.yaml ansible/playbook.yml $(find ansible/roles -name '*.yml'); do
  if python3 -c "import yaml,sys; yaml.safe_load(open('$f'))" 2>/dev/null; then
    printf "  ok    %s\n" "$f"
  else
    printf "  FAIL  %s\n" "$f"; python3 -c "import yaml; yaml.safe_load(open('$f'))" 2>&1 | tail -2
    fail=$((fail+1))
  fi
done

echo ""
echo "=== HCL brace balance ==="
for f in tofu/*.tf; do
  o=$(tr -cd '{' < "$f" | wc -c); c=$(tr -cd '}' < "$f" | wc -c)
  [ "$o" -eq "$c" ] && printf "  ok    %-16s %s pairs\n" "$(basename $f)" "$o" \
    || { printf "  FAIL  %-16s %s vs %s\n" "$(basename $f)" "$o" "$c"; fail=$((fail+1)); }
done

echo ""
echo "=== variables declared vs referenced ==="
declared=$(grep -hoE '^variable "[a-z0-9_]+"' tofu/variables.tf | sed 's/variable "//; s/"//' | sort -u)
used=$(grep -hoE 'var\.[a-z0-9_]+' tofu/*.tf | sed 's/var\.//' | sort -u)
for u in $used; do echo "$declared" | grep -qx "$u" || { echo "  FAIL undeclared var.$u"; fail=$((fail+1)); }; done
echo "  declared $(echo "$declared" | wc -l), referenced $(echo "$used" | wc -l)"

echo ""
echo "=== resource references resolve ==="
res=$(grep -hoE '^resource "[a-z0-9_]+" "[a-z0-9_]+"' tofu/*.tf | sed 's/resource "//; s/" "/./; s/"//' | sort -u)
for r in $(grep -hoE '\bhcloud_[a-z0-9_]+\.[a-z0-9_]+' tofu/*.tf | sort -u); do
  echo "$res" | grep -qx "$r" || { echo "  FAIL dangling $r"; fail=$((fail+1)); }
done
echo "  $(echo "$res" | wc -l) resources, references resolve"

echo ""
echo "=== roles referenced by the playbook exist ==="
for r in $(grep -oE '^\s+- role: [a-z]+' ansible/playbook.yml | awk '{print $3}'); do
  [ -f "ansible/roles/$r/tasks/main.yml" ] && echo "  ok    role $r" \
    || { echo "  FAIL  role $r has no tasks/main.yml"; fail=$((fail+1)); }
done

echo ""
echo "=== safety guards ==="
grep -q "prevent_destroy" tofu/server.tf && echo "  ok    workspace volume prevent_destroy" || { echo "  FAIL"; fail=$((fail+1)); }
grep -q 'firewall_ids' tofu/server.tf && echo "  ok    firewall attached at creation" || { echo "  FAIL"; fail=$((fail+1)); }
grep -qE 'source_ips\s*=\s*\["0\.0\.0\.0/0"\]' tofu/firewall.tf && { echo "  FAIL  world-open SSH"; fail=$((fail+1)); } || echo "  ok    no world-open inbound"
grep -q 'PasswordAuthentication no' cloud-init/bootstrap.yaml && echo "  ok    password auth disabled" || { echo "  FAIL"; fail=$((fail+1)); }
grep -q 'unity_licence_confirmed' ansible/roles/unity/tasks/main.yml && echo "  ok    Unity install gated on licence decision" || { echo "  FAIL"; fail=$((fail+1)); }
grep -rqiE '(hcloud_token|api_token)[[:space:]]*=[[:space:]]*"[A-Za-z0-9]' tofu/ && { echo "  FAIL  literal token"; fail=$((fail+1)); } || echo "  ok    no literal tokens"

echo ""
if [ $fail -eq 0 ]; then
  echo "ALL CHECKS PASSED"
  echo ""
  echo "NOT verified here (needs a real HCLOUD_TOKEN):"
  echo "  - tofu init / validate / plan against the provider"
  echo "  - that server_type '$(awk '/variable "server_type"/,/^}/' tofu/variables.tf | grep -E '^\s+default' | head -1 | grep -oE '"[a-z0-9]+"' | tr -d '"')' exists: run 'hcloud server-type list'"
else
  echo "$fail FAILURE(S)"
fi
exit $fail
