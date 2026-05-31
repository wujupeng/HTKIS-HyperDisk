import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Enumerate the WDS default BCD to see its config
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
bcdedit /store "C:\RemoteInstall\Boot\x64uefi\default.bcd" /enum all
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("WDS x64uefi BCD:")
print(out[:3000])
print("Status:", r.status_code)
