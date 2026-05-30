#!/bin/bash
# B3-T01 + B3-T02: Deploy dnsmasq + iPXE on Debian 13
# Configures PXE boot service (DHCP+TFTP) and iPXE boot files
set -euo pipefail

PXE_IP="10.10.100.10"
STORAGE_IP="10.10.200.10"
TFTP_ROOT="/opt/hyperdisk/tftp"
BOOT_ROOT="/opt/hyperdisk/static/boot"

echo "=== B3-T01: Installing dnsmasq ==="
if ! command -v dnsmasq &>/dev/null; then
    apt-get update -qq
    apt-get install -y -qq dnsmasq
fi
echo "dnsmasq installed: $(dnsmasq --version | head -1)"

echo "=== B3-T01: Deploying dnsmasq configuration ==="
cp deploy/dnsmasq/dnsmasq.conf /etc/dnsmasq.d/hyperdisk-pxe.conf

echo "=== B3-T01: Creating TFTP root directory ==="
mkdir -p "${TFTP_ROOT}"
chown dnsmasq:dnsmasq "${TFTP_ROOT}"

echo "=== B3-T02: Downloading iPXE UEFI binary ==="
IPXE_EFI="${TFTP_ROOT}/ipxe.efi"
if [ ! -f "${IPXE_EFI}" ]; then
    curl -fsSL -o "${IPXE_EFI}" \
        "https://boot.ipxe.org/ipxe.efi"
    chmod 644 "${IPXE_EFI}"
fi
IPXE_SIZE=$(stat -f%z "${IPXE_EFI}" 2>/dev/null || stat -c%s "${IPXE_EFI}")
echo "ipxe.efi size: ${IPXE_SIZE} bytes (must be < 1MB)"

echo "=== B3-T02: Deploying iPXE boot scripts ==="
mkdir -p "${BOOT_ROOT}"
cp deploy/pxe/boot/default.ipxe "${BOOT_ROOT}/default.ipxe"
chmod 644 "${BOOT_ROOT}/default.ipxe"
echo "Boot scripts deployed to ${BOOT_ROOT}"

echo "=== B3-T02: Updating Nginx for iPXE HTTP boot ==="
if ! grep -q "location /boot/" /etc/nginx/sites-available/hyperdisk; then
    sed -i '/location \/ {/i\
    location /boot/ {\
        alias /opt/hyperdisk/static/boot/;\
        expires 5m;\
    }' /etc/nginx/sites-available/hyperdisk
    nginx -t && systemctl reload nginx
    echo "Nginx updated with /boot/ location"
else
    echo "Nginx already has /boot/ location"
fi

echo "=== B3-T01: Restarting dnsmasq ==="
systemctl stop dnsmasq 2>/dev/null || true
systemctl start dnsmasq
systemctl enable dnsmasq

echo "=== Verification ==="
echo "dnsmasq status: $(systemctl is-active dnsmasq)"
echo "TFTP root: ${TFTP_ROOT}"
echo "  ipxe.efi: $([ -f ${IPXE_EFI} ] && echo 'OK' || echo 'MISSING')"
echo "Boot scripts: ${BOOT_ROOT}"
echo "  default.ipxe: $([ -f ${BOOT_ROOT}/default.ipxe ] && echo 'OK' || echo 'MISSING')"
echo ""
echo "B3-T01 + B3-T02 deployment complete!"
echo "PXE boot chain: UEFI PXE -> DHCP(${PXE_IP}) -> TFTP(ipxe.efi) -> HTTP(default.ipxe) -> BootAgent"
