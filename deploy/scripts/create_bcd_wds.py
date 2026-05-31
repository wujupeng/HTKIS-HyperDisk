import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Copy WDS BCD and add WinPE OS loader entry
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    $src = 'C:\RemoteInstall\Boot\x64uefi\default.bcd'
    $dst = 'C:\bootwim_temp\BCD_wds'
    Copy-Item $src $dst -Force
    
    # Add WinPE OS loader
    $output = bcdedit /store $dst /create /d "Windows PE" /application osloader
    $guid = ($output -split ' ')[2]
    Write-Output "GUID: $guid"
    
    # Use WDS's existing ramdiskoptions {68d9e51c-a129-4ee1-9725-2ab00a957daf}
    $ramguid = '{68d9e51c-a129-4ee1-9725-2ab00a957daf}'
    
    bcdedit /store $dst /set $guid device ramdisk=[boot]\Boot\boot.wim,$ramguid
    bcdedit /store $dst /set $guid osdevice ramdisk=[boot]\Boot\boot.wim,$ramguid
    bcdedit /store $dst /set $guid systemroot \Windows
    bcdedit /store $dst /set $guid winpe yes
    bcdedit /store $dst /set $guid detecthal yes
    bcdedit /store $dst /set $guid nx optin
    bcdedit /store $dst /set $guid pae Default
    Write-Output "Set OS loader"
    
    # Add to bootmgr displayorder
    bcdedit /store $dst /displayorder $guid /addlast
    bcdedit /store $dst /set `{bootmgr`} timeout 0
    Write-Output "Set displayorder"
    
    # Enum final BCD
    Write-Output "=== Final BCD ==="
    bcdedit /store $dst /enum all
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:3000])
print("Status:", r.status_code)
