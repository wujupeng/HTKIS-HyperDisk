#!/bin/bash
ALPHA_A_LOG="/var/log/hyperdisk/alpha_a_pxe.log"
DHCP_LOG="/var/log/dnsmasq-dhcp.log"

mkdir -p /var/log/hyperdisk

echo "[$(date)] Alpha-A PXE Monitor Started" > "$ALPHA_A_LOG"
echo "Watching dnsmasq DHCP requests..." >> "$ALPHA_A_LOG"

tail -f /var/log/syslog 2>/dev/null | while read line; do
    if echo "$line" | grep -q "dnsmasq.*DHCPDISCOVER"; then
        mac=$(echo "$line" | grep -oP 'DHCPDISCOVER \K[0-9a-f:]+')
        echo "[$(date)] DHCPDISCOVER from $mac" >> "$ALPHA_A_LOG"
    fi
    if echo "$line" | grep -q "dnsmasq.*DHCPOFFER"; then
        echo "[$(date)] DHCPOFFER sent" >> "$ALPHA_A_LOG"
    fi
    if echo "$line" | grep -q "dnsmasq.*DHCPREQUEST"; then
        echo "[$(date)] DHCPREQUEST received" >> "$ALPHA_A_LOG"
    fi
    if echo "$line" | grep -q "dnsmasq.*DHCPACK"; then
        ip=$(echo "$line" | grep -oP 'DHCPACK \K[0-9.]+')
        echo "[$(date)] DHCPACK to $ip" >> "$ALPHA_A_LOG"
    fi
    if echo "$line" | grep -q "dnsmasq.*tftp"; then
        file=$(echo "$line" | grep -oP 'tftp.*\K[^ ]+')
        echo "[$(date)] TFTP: $file" >> "$ALPHA_A_LOG"
    fi
done
