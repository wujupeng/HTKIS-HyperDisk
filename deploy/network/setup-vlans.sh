#!/bin/bash
# B1-ENV-02: Three-Network VLAN Configuration for Debian 13
# PXE VLAN 100 | Storage VLAN 200 | Management VLAN 300

set -euo pipefail

IFACE="${1:-eno1}"

cat > /etc/network/interfaces.d/hyperdisk-vlans <<EOF
# HyperDisk X - Three Network VLANs
# PXE Network (VLAN 100)
auto ${IFACE}.100
iface ${IFACE}.100 inet static
    address 10.10.100.10
    netmask 255.255.255.0
    vlan-raw-device ${IFACE}

# Storage Network (VLAN 200)
auto ${IFACE}.200
iface ${IFACE}.200 inet static
    address 10.10.200.10
    netmask 255.255.255.0
    vlan-raw-device ${IFACE}

# Management Network (VLAN 300)
auto ${IFACE}.300
iface ${IFACE}.300 inet static
    address 10.10.300.10
    netmask 255.255.255.0
    vlan-raw-device ${IFACE}
EOF

ip link add link ${IFACE} name ${IFACE}.100 type vlan id 100 2>/dev/null || true
ip link add link ${IFACE} name ${IFACE}.200 type vlan id 200 2>/dev/null || true
ip link add link ${IFACE} name ${IFACE}.300 type vlan id 300 2>/dev/null || true

ip addr add 10.10.100.10/24 dev ${IFACE}.100 2>/dev/null || true
ip addr add 10.10.200.10/24 dev ${IFACE}.200 2>/dev/null || true
ip addr add 10.10.300.10/24 dev ${IFACE}.300 2>/dev/null || true

ip link set ${IFACE}.100 up
ip link set ${IFACE}.200 up
ip link set ${IFACE}.300 up

echo "VLAN configuration applied. Verifying..."
for vlan in 100 200 300; do
    if ip addr show ${IFACE}.${vlan} | grep -q "inet "; then
        echo "  VLAN ${vlan}: OK"
    else
        echo "  VLAN ${vlan}: FAILED"
        exit 1
    fi
done
echo "All VLANs configured successfully."
