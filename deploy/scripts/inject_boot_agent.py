import paramiko
import os
import time

HOST = "192.168.2.80"
USER = "debian"
PASS = "9090"

LOCAL_EXE = "D:/DEV/HTKIS-HyperDisk/build_boot_agent/client/boot_agent/hd_boot_agent.exe"
REMOTE_EXE = "/tmp/hd_boot_agent.exe"
WIM_PATH = "/opt/hyperdisk/static/boot/winpe/boot.wim"
WIM_MOUNT = "/tmp/wim_mount"

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
        print(f"[STDERR] {err.strip()[-1000:]}")
    return exit_code, out, err

def main():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PASS, timeout=10)
    print("[OK] SSH connected")
    
    sftp = ssh.open_sftp()
    
    # Step 1: Upload hd_boot_agent.exe
    print(f"\n[UPLOAD] {LOCAL_EXE} -> {REMOTE_EXE}")
    sftp.put(LOCAL_EXE, REMOTE_EXE)
    sftp.close()
    ssh_exec(ssh, f"ls -la {REMOTE_EXE}")
    
    # Step 2: Check wimlib-imagex availability
    ssh_exec(ssh, "which wimlib-imagex 2>/dev/null || which imagex 2>/dev/null || dpkg -l | grep wimlib")
    
    # Step 3: Mount WIM, replace exe, unmount
    # Create mount point
    ssh_exec(ssh, f"mkdir -p {WIM_MOUNT}", sudo=True)
    
    # Mount the WIM (image index 1)
    print("\n[MOUNT] Mounting WIM...")
    rc, out, err = ssh_exec(ssh, f"wimlib-imagex mount {WIM_PATH} {WIM_MOUNT} --image=1", sudo=True, timeout=60)
    
    if rc != 0:
        print("[MOUNT FAILED] Trying alternate approach: update in place")
        # Try wimlib-imagex update instead
        # First create a temp directory with the new exe
        ssh_exec(ssh, "mkdir -p /tmp/wim_update/Windows/System32", sudo=True)
        ssh_exec(ssh, f"cp {REMOTE_EXE} /tmp/wim_update/Windows/System32/hd_boot_agent.exe", sudo=True)
        rc2, _, _ = ssh_exec(ssh, f"wimlib-imagex update {WIM_PATH} --command='add /tmp/wim_update/Windows/System32/hd_boot_agent.exe /Windows/System32/hd_boot_agent.exe'", sudo=True, timeout=120)
        if rc2 != 0:
            print("[UPDATE FAILED] Will try direct wimlib-imagex directory add")
        else:
            print("[UPDATE OK]")
    else:
        # Mounted successfully - replace the exe
        print("\n[REPLACE] Replacing hd_boot_agent.exe in mounted WIM...")
        ssh_exec(ssh, f"ls -la {WIM_MOUNT}/Windows/System32/hd_boot_agent.exe", sudo=True)
        ssh_exec(ssh, f"cp {REMOTE_EXE} {WIM_MOUNT}/Windows/System32/hd_boot_agent.exe", sudo=True)
        ssh_exec(ssh, f"ls -la {WIM_MOUNT}/Windows/System32/hd_boot_agent.exe", sudo=True)
        
        # Unmount and commit
        print("\n[UNMOUNT] Unmounting WIM (committing changes)...")
        ssh_exec(ssh, f"wimlib-imagex unmount {WIM_MOUNT} --commit", sudo=True, timeout=120)
    
    # Step 4: Verify WIM
    print("\n[VERIFY] Checking WIM info...")
    ssh_exec(ssh, f"wimlib-imagex info {WIM_PATH}", sudo=True)
    
    # Step 5: Restart nginx to ensure files are served fresh
    ssh_exec(ssh, "systemctl restart nginx", sudo=True)
    time.sleep(2)
    ssh_exec(ssh, "systemctl status nginx --no-pager | head -10", sudo=True)
    
    # Step 6: Verify the exe is accessible via HTTP
    ssh_exec(ssh, "curl -sI http://127.0.0.1:8080/boot/winpe/boot.wim | head -5")
    
    ssh.close()
    print("\n[DONE]")

if __name__ == "__main__":
    main()
