import subprocess
import os

MSVC = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
SDK = r"C:\Program Files (x86)\Windows Kits\10"
SV = "10.0.26100.0"
CL = os.path.join(MSVC, "bin", "Hostx64", "x64", "cl.exe")
ROOT = r"D:\DEV\HTKIS-HyperDisk"

cmd = [
    CL, "/MT", "/O2", "/D_WIN32", "/D_AMD64_",
    f"/I{SDK}\\Include\\{SV}\\um",
    f"/I{SDK}\\Include\\{SV}\\shared",
    f"/I{SDK}\\Include\\{SV}\\ucrt",
    f"/I{MSVC}\\include",
    os.path.join(ROOT, "tools", "hd_ld.c"),
    f"/Fo{ROOT}\\tools\\hd_ld.obj",
    f"/Fe{ROOT}\\tools\\hd_ld.exe",
    "/link",
    f"/LIBPATH:{SDK}\\Lib\\{SV}\\um\\x64",
    f"/LIBPATH:{SDK}\\Lib\\{SV}\\ucrt\\x64",
    f"/LIBPATH:{MSVC}\\lib\\x64",
    "ntdll.lib",
]

result = subprocess.run(cmd, capture_output=True)
print(result.stdout.decode("utf-8", errors="replace")[-500:])
print(result.stderr.decode("utf-8", errors="replace")[-500:])
print(f"rc={result.returncode}")

exe = os.path.join(ROOT, "tools", "hd_ld.exe")
if os.path.exists(exe):
    print(f"hd_ld.exe: {os.path.getsize(exe)} bytes")
