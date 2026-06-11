import paramiko
import os
import time

HOST = "192.168.2.80"
USER = "debian"
PASS = "9090"

DRIVER_BUILD = "D:/DEV/HTKIS-HyperDisk/driver/build"
DRIVER_SRC = "D:/DEV/HTKIS-HyperDisk/driver"
BOOT_DIR = "D:/DEV/HTKIS-HyperDisk/deploy/pxe/boot"

WIM_PATH = "/opt/hyperdisk/static/boot/winpe/boot.wim"
WIM_MOUNT = "/tmp/wim_mount"
WIM_IMAGE = 2

FILES_TO_INJECT = [
    (f"{DRIVER_BUILD}/HDBus.sys",           "/Windows/System32/drivers/HDBus.sys"),
    (f"{DRIVER_BUILD}/HDBlk.sys",           "/Windows/System32/drivers/HDBlk.sys"),
    (f"{DRIVER_BUILD}/HDNet.sys",           "/Windows/System32/drivers/HDNet.sys"),
    (f"{DRIVER_BUILD}/HyperCache.sys",      "/Windows/System32/drivers/HyperCache.sys"),
    (f"{DRIVER_BUILD}/HyperOverlay.sys",    "/Windows/System32/drivers/HyperOverlay.sys"),
    (f"{DRIVER_SRC}/bus/HyperDiskBus.inf",  "/Windows/System32/drivers/HyperDiskBus.inf"),
    (f"{DRIVER_SRC}/blk/HyperDiskBlk.inf",  "/Windows/System32/drivers/HyperDiskBlk.inf"),
    (f"{DRIVER_SRC}/net/HyperNet.inf",      "/Windows/System32/drivers/HyperNet.inf"),
    (f"{BOOT_DIR}/startnet_v14_stable.cmd", "/Windows/System32/startnet.cmd"),
    (f"{BOOT_DIR}/hd_dp.txt",              "/Windows/System32/hd_dp.txt"),
]

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
    for local, _ in FILES_TO_INJECT:
        if not os.path.exists(local):
            print(f"[FATAL] Missing local file: {local}")
            return

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(HOST, username=USER, password=PASS, timeout=10)
    print("[OK] SSH connected")

    sftp = ssh.open_sftp()

    upload_dir = "/home/debian/b1_inject"
    print(f"\n[1/6] Uploading files to {upload_dir}/...")
    ssh_exec(ssh, f"mkdir -p {upload_dir}")
    for local, remote in FILES_TO_INJECT:
        basename = os.path.basename(remote)
        tmp_remote = f"{upload_dir}/{basename}"
        sftp.put(local, tmp_remote)
        print(f"  {basename} -> {tmp_remote}")
    sftp.close()

    print("\n[2/6] Verifying uploaded files...")
    ssh_exec(ssh, f"ls -la {upload_dir}/")

    print(f"\n[3/6] Mounting WIM Image {WIM_IMAGE}...")
    ssh_exec(ssh, f"mkdir -p {WIM_MOUNT}", sudo=True)
    rc, out, err = ssh_exec(ssh, f"fusermount -u {WIM_MOUNT} 2>/dev/null", sudo=True, timeout=30)
    time.sleep(1)

    rc, out, err = ssh_exec(ssh, f"wimlib-imagex mount {WIM_PATH} {WIM_IMAGE} {WIM_MOUNT}", sudo=True, timeout=60)
    if rc != 0:
        print("[MOUNT FAILED] Trying wimlib-imagex update approach...")
        cmdfile_lines = []
        for local, remote in FILES_TO_INJECT:
            basename = os.path.basename(remote)
            cmdfile_lines.append(f"add {upload_dir}/{basename} {remote}")
        cmdfile_content = "\n".join(cmdfile_lines) + "\n"
        cmdfile_remote = f"{upload_dir}/update.cmd"
        sftp2 = ssh.open_sftp()
        with sftp2.open(cmdfile_remote, 'w') as f:
            f.write(cmdfile_content)
        sftp2.close()
        print(f"  Update commands:\n{cmdfile_content}")
        rc2, out2, err2 = ssh_exec(ssh, f"wimlib-imagex update {WIM_PATH} {WIM_IMAGE} < {cmdfile_remote}", sudo=True, timeout=120)
        if rc2 != 0:
            print(f"  [UPDATE FAILED] rc={rc2}")
            print(f"  stdout: {out2[-500:]}")
            print(f"  stderr: {err2[-500:]}")
        else:
            print("  [UPDATE OK]")
    else:
        print(f"\n[4/6] Copying files into mounted WIM...")
        ssh_exec(ssh, f"mkdir -p {WIM_MOUNT}/Windows/System32/drivers", sudo=True)
        for local, remote in FILES_TO_INJECT:
            basename = os.path.basename(remote)
            ssh_exec(ssh, f"cp {upload_dir}/{basename} {WIM_MOUNT}{remote}", sudo=True)
            print(f"  {basename} -> {remote}")

        print("\n[5/6] Verifying files in WIM...")
        ssh_exec(ssh, f"ls -la {WIM_MOUNT}/Windows/System32/drivers/HD*", sudo=True)
        ssh_exec(ssh, f"ls -la {WIM_MOUNT}/Windows/System32/startnet.cmd", sudo=True)

        print("\n[6/6] Unmounting WIM (committing changes)...")
        rc3, _, _ = ssh_exec(ssh, f"wimlib-imagex unmount {WIM_MOUNT} --commit", sudo=True, timeout=120)
        if rc3 != 0:
            print("[UNMOUNT FAILED!]")
            return

    print("\n[VERIFY] WIM info...")
    ssh_exec(ssh, f"wimlib-imagex info {WIM_PATH}", sudo=True)

    print("\n[CLEANUP] Restarting nginx...")
    ssh_exec(ssh, "systemctl restart nginx", sudo=True)
    time.sleep(2)
    ssh_exec(ssh, "systemctl status nginx --no-pager | head -5", sudo=True)

    print("\n[CLEANUP] Removing temp files...")
    ssh_exec(ssh, f"rm -rf {upload_dir}", sudo=True)

    ssh.close()
    print("\n[DONE] B1 drivers injected into WIM Image 2")

if __name__ == "__main__":
    main()
