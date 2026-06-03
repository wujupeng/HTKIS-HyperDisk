import paramiko
import time

HOST = "192.168.2.80"
USER = "debian"
PASS = "9090"
REMOTE_BASE = "/tmp/hd_build/server"

def ssh_exec(ssh, cmd, timeout=120, sudo=False):
    if sudo:
        cmd = f"echo '{PASS}' | sudo -S {cmd}"
    print(f"[CMD] {cmd}")
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout, get_pty=sudo)
    out = stdout.read().decode('utf-8', errors='replace')
    err = stderr.read().decode('utf-8', errors='replace')
    exit_code = stdout.channel.recv_exit_status()
    if sudo and out.strip():
        lines = out.strip().split('\n')
        filtered = [l for l in lines if '[sudo]' not in l and 'password' not in l.lower()]
        if filtered:
            print('\n'.join(filtered[-20:]))
    elif out.strip():
        print(out.strip()[-2000:])
    if err.strip() and not sudo:
        print(f"[STDERR] {err.strip()[-2000:]}")
    return exit_code, out, err

def main():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PASS, timeout=10)
    print("[OK] SSH connected")

    # Verify binary exists
    ssh_exec(ssh, f"ls -la {REMOTE_BASE}/target/release/hd-api-gateway")

    # Stop service
    ssh_exec(ssh, "systemctl stop hyperdisk-gateway 2>/dev/null || true", sudo=True)
    
    # Copy binary
    ssh_exec(ssh, f"cp {REMOTE_BASE}/target/release/hd-api-gateway /opt/hyperdisk/bin/hd-api-gateway", sudo=True)
    
    # Verify copy
    ssh_exec(ssh, "ls -la /opt/hyperdisk/bin/hd-api-gateway", sudo=True)
    
    # Start service
    ssh_exec(ssh, "systemctl start hyperdisk-gateway", sudo=True)
    time.sleep(3)
    
    # Check status
    ssh_exec(ssh, "systemctl status hyperdisk-gateway --no-pager -l 2>/dev/null | head -20", sudo=True)
    
    # Verify endpoints
    print("\n[VERIFY] Testing endpoints...")
    ssh_exec(ssh, "curl -s http://127.0.0.1:8080/api/v1/boot/sessions")
    ssh_exec(ssh, "curl -s http://127.0.0.1:8080/api/v1/images")
    ssh_exec(ssh, "curl -s -X POST http://127.0.0.1:8080/api/v1/boot/report -H 'Content-Type: application/json' -d '{\"boot_id\":\"test-001\",\"machine_id\":\"test-mac\",\"mac_address\":\"00:11:22:33:44:55\",\"phase\":\"pxe\",\"duration_ms\":100,\"result\":\"ok\"}'")
    time.sleep(1)
    ssh_exec(ssh, "curl -s http://127.0.0.1:8080/api/v1/boot/sessions")
    
    ssh.close()

if __name__ == "__main__":
    main()
