import paramiko
import os
import sys
import time

HOST = "192.168.2.80"
USER = "debian"
PASS = "9090"
REMOTE_BASE = "/tmp/hd_build/server"
LOCAL_SERVER = "D:/DEV/HTKIS-HyperDisk/server"

WORKSPACE_MEMBERS = ["metadata_center", "update_service", "dna_service", "api_gateway", "common"]

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

def upload_dir(sftp, local_dir, remote_dir):
    try:
        sftp.stat(remote_dir)
    except FileNotFoundError:
        sftp.mkdir(remote_dir)
    
    for item in os.listdir(local_dir):
        local_path = os.path.join(local_dir, item)
        remote_path = f"{remote_dir}/{item}"
        
        if os.path.isdir(local_path):
            if item in ('target', '.git', 'node_modules'):
                continue
            upload_dir(sftp, local_path, remote_path)
        else:
            print(f"  Upload: {local_path} -> {remote_path}")
            sftp.put(local_path, remote_path)

def main():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PASS, timeout=10)
    print("[OK] SSH connected")
    
    sftp = ssh.open_sftp()
    print("[OK] SFTP opened")
    
    # Step 1: Clean remote and create base dir
    ssh_exec(ssh, f"rm -rf {REMOTE_BASE} && mkdir -p {REMOTE_BASE}")
    
    # Step 2: Upload workspace Cargo.toml
    local_cargo = os.path.join(LOCAL_SERVER, "Cargo.toml")
    remote_cargo = f"{REMOTE_BASE}/Cargo.toml"
    print(f"Upload: {local_cargo} -> {remote_cargo}")
    sftp.put(local_cargo, remote_cargo)
    
    # Step 3: Upload Cargo.lock if exists
    local_lock = os.path.join(LOCAL_SERVER, "Cargo.lock")
    if os.path.exists(local_lock):
        print(f"Upload: {local_lock} -> {REMOTE_BASE}/Cargo.lock")
        sftp.put(local_lock, f"{REMOTE_BASE}/Cargo.lock")
    
    # Step 4: Upload each workspace member
    for member in WORKSPACE_MEMBERS:
        local_member = os.path.join(LOCAL_SERVER, member)
        remote_member = f"{REMOTE_BASE}/{member}"
        if os.path.isdir(local_member):
            print(f"\n[Upload member: {member}]")
            upload_dir(sftp, local_member, remote_member)
    
    # Step 5: Also upload .cargo/config.toml if exists
    local_cargo_config = os.path.join(LOCAL_SERVER, ".cargo", "config.toml")
    if os.path.exists(local_cargo_config):
        ssh_exec(ssh, f"mkdir -p {REMOTE_BASE}/.cargo")
        sftp.put(local_cargo_config, f"{REMOTE_BASE}/.cargo/config.toml")
    
    sftp.close()
    print("\n[OK] All files uploaded")
    
    # Step 6: Verify uploaded structure
    ssh_exec(ssh, f"find {REMOTE_BASE} -name 'Cargo.toml' | sort")
    
    # Step 7: Build api_gateway on Debian
    print("\n[BUILD] Starting cargo build for api_gateway...")
    exit_code, out, err = ssh_exec(ssh, f"cd {REMOTE_BASE} && cargo build --release -p hd-api-gateway 2>&1", timeout=600)
    
    if exit_code == 0:
        print("\n[BUILD SUCCESS]")
        # Step 8: Deploy - stop service, copy binary, restart
        ssh_exec(ssh, "systemctl stop hyperdisk-gateway 2>/dev/null || true", sudo=True)
        ssh_exec(ssh, f"cp {REMOTE_BASE}/target/release/hd-api-gateway /opt/hyperdisk/bin/hd-api-gateway", sudo=True)
        ssh_exec(ssh, "systemctl start hyperdisk-gateway", sudo=True)
        time.sleep(3)
        ssh_exec(ssh, "systemctl status hyperdisk-gateway --no-pager -l 2>/dev/null | head -20", sudo=True)
        # Step 9: Verify endpoints
        ssh_exec(ssh, "curl -s http://127.0.0.1:8080/api/v1/boot/sessions 2>/dev/null || echo 'sessions endpoint not ready'")
    else:
        print(f"\n[BUILD FAILED] exit code: {exit_code}")
        # Print more error details
        if err.strip():
            print(f"Error details:\n{err[-3000:]}")
    
    ssh.close()

if __name__ == "__main__":
    main()
