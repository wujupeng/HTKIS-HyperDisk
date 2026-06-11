import subprocess
import os
import sys

WDK_ROOT = r"C:\Program Files (x86)\Windows Kits\10"
WDK_VER = "10.0.26100.0"
MSVC_ROOT = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
DRIVER_ROOT = os.path.dirname(os.path.abspath(__file__))
OUTDIR = os.path.join(DRIVER_ROOT, "build")

WDK_INC = os.path.join(WDK_ROOT, "Include", WDK_VER)
WDK_LIB = os.path.join(WDK_ROOT, "Lib", WDK_VER)
MSVC_INC = os.path.join(MSVC_ROOT, "include")
MSVC_LIB = os.path.join(MSVC_ROOT, "lib", "x64")

CL = os.path.join(MSVC_ROOT, "bin", "Hostx64", "x64", "cl.exe")
LIB = os.path.join(MSVC_ROOT, "bin", "Hostx64", "x64", "lib.exe")
LINK = os.path.join(MSVC_ROOT, "bin", "Hostx64", "x64", "link.exe")

CFLAGS = [
    "/kernel", "/GS-", "/GL-", "/MP", "/W3", "/WX-",
    "/Zi", "/Od", "/Oi", "/Oy-", "/c",
    "/D_WIN32", "/D_AMD64_",
    "/DNTDDI_VERSION=0x0A000007", "/DWINVER=0x0A00", "/D_WIN32_WINNT=0x0A00",
    "/DPOOL_ZEROING=1",
]

INCLUDES = [
    f'/I{os.path.join(WDK_INC, "km")}',
    f'/I{os.path.join(WDK_INC, "shared")}',
    f'/I{os.path.join(WDK_INC, "ucrt")}',
    f'/I{os.path.join(MSVC_INC)}',
]

LFLAGS = [
    "/NODEFAULTLIB", "/SUBSYSTEM:NATIVE", "/DRIVER", "/KERNEL",
    "/ENTRY:DriverEntry",
    "/MERGE:.text=.PAGE", "/MERGE:.data=.PAGE",
    "/SECTION:INIT,d", "/SECTION:PAGE,d",
    "/IGNORE:4197", "/IGNORE:4010", "/IGNORE:4078", "/IGNORE:4221",
    "/DEBUG",
    f'/LIBPATH:{os.path.join(WDK_LIB, "km", "x64")}',
    f'/LIBPATH:{MSVC_LIB}',
]

def run(cmd, desc):
    print(f"\n{'='*60}")
    print(f"  {desc}")
    print(f"{'='*60}")
    result = subprocess.run(cmd, capture_output=True)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")
    if stdout:
        print(stdout)
    if stderr:
        print(stderr)
    if result.returncode != 0:
        print(f"[FAIL] {desc} (exit code {result.returncode})")
        return False
    print(f"[OK] {desc}")
    return True

def main():
    os.makedirs(OUTDIR, exist_ok=True)

    print("=" * 60)
    print("  HyperDisk WDK Driver Build System")
    print(f"  WDK: {WDK_VER}  Target: x64 KM")
    print("=" * 60)

    build_ok = True

    # Step 1: Common library
    common_out = os.path.join(OUTDIR, "common")
    os.makedirs(common_out, exist_ok=True)
    common_srcs = [
        os.path.join(DRIVER_ROOT, "common", f)
        for f in ["hd_debug.c", "hd_memory.c", "hd_lock.c", "hd_registry.c", "hd_serial.c"]
    ]
    cmd = [CL] + CFLAGS + INCLUDES + [f"/Fo{common_out}\\"] + common_srcs
    if not run(cmd, "[1/6] Compiling hd_driver_common"):
        build_ok = False
        return

    common_objs = [os.path.join(common_out, f + ".obj") for f in
                   ["hd_debug", "hd_memory", "hd_lock", "hd_registry", "hd_serial"]]
    common_lib = os.path.join(OUTDIR, "hd_driver_common.lib")
    cmd = [LIB, f"/OUT:{common_lib}"] + common_objs
    for o in common_objs:
        if not os.path.exists(o):
            print(f"[WARN] Missing obj: {o}")
    if not run(cmd, "Archiving hd_driver_common.lib"):
        build_ok = False
        return

    # Step 2: HDBus.sys
    drivers = [
        {
            "name": "HDBus",
            "dir": "bus",
            "sources": ["bus_driver.c", "bus_pnp.c", "bus_ioctl.c", "bus_child.c"],
            "libs": ["ntoskrnl.lib", "hal.lib", "ntstrsafe.lib"],
        },
        {
            "name": "HDBlk",
            "dir": "blk",
            "sources": ["blk_driver.c", "blk_io.c", "blk_ioctl.c", "blk_request.c",
                        "blk_irp_tracker.c", "blk_fastio.c", "blk_cc_mm_sync.c",
                        "blk_pool.c", "blk_7b_recovery.c"],
            "libs": ["ntoskrnl.lib", "hal.lib"],
        },
        {
            "name": "HDNet",
            "dir": "net",
            "sources": ["net_driver.c", "net_wsk.c", "net_frame.c", "net_connect.c", "net_heartbeat.c"],
            "libs": ["ntoskrnl.lib", "hal.lib", "netio.lib"],
        },
        {
            "name": "HyperCache",
            "dir": "cache",
            "sources": ["cache_entry.c", "cache_driver.c"],
            "libs": ["ntoskrnl.lib", "hal.lib", "fltMgr.lib"],
        },
        {
            "name": "HyperOverlay",
            "dir": "overlay",
            "sources": ["overlay_entry.c", "overlay_driver.c"],
            "libs": ["ntoskrnl.lib", "hal.lib", "fltMgr.lib"],
        },
    ]

    for i, drv in enumerate(drivers):
        step = i + 2
        drv_out = os.path.join(OUTDIR, drv["dir"])
        os.makedirs(drv_out, exist_ok=True)

        drv_includes = INCLUDES + [
            f'/I{os.path.join(DRIVER_ROOT, "include")}',
            f'/I{os.path.join(DRIVER_ROOT, "common")}',
            f'/I{os.path.join(DRIVER_ROOT, drv["dir"])}',
        ]
        drv_srcs = [os.path.join(DRIVER_ROOT, drv["dir"], f) for f in drv["sources"]]

        cmd = [CL] + CFLAGS + drv_includes + [f"/Fo{drv_out}\\"] + drv_srcs
        if not run(cmd, f"[{step}/6] Compiling {drv['name']}.sys"):
            build_ok = False
            return

        objs = [os.path.join(drv_out, os.path.splitext(f)[0] + ".obj") for f in drv["sources"]]
        sys_path = os.path.join(OUTDIR, f"{drv['name']}.sys")
        cmd = [LINK] + LFLAGS + [f"/OUT:{sys_path}"] + objs + \
              [os.path.join(OUTDIR, "hd_driver_common.lib")] + drv["libs"]
        if not run(cmd, f"Linking {drv['name']}.sys"):
            build_ok = False
            return

    # Summary
    print("\n" + "=" * 60)
    print("  Build Summary")
    print("=" * 60)
    if build_ok:
        sys_files = [f for f in os.listdir(OUTDIR) if f.endswith(".sys")]
        total = 0
        for f in sys_files:
            size = os.path.getsize(os.path.join(OUTDIR, f))
            total += size
            print(f"  [OK] {f:30s} {size:>8,} bytes")
        print(f"\n  {len(sys_files)} drivers built, total {total:,} bytes")
    else:
        print("  BUILD FAILED")
    print("=" * 60)

if __name__ == "__main__":
    main()
