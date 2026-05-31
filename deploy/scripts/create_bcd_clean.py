import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Create clean minimal BCD for PXE boot
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    $bcdPath = 'C:\bootwim_temp\BCD_clean'
    if (Test-Path $bcdPath) { Remove-Item $bcdPath -Force }
    
    # Fresh store
    bcdedit /createstore $bcdPath
    
    # ramdiskoptions - this is critical for PXE/TFTP
    bcdedit /store $bcdPath /create `{ramdiskoptions`} /d "Ramdisk Options"
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisksdidevice boot
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisksdipath \Boot\Boot.SDI
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisktftpblocksize 1456
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisktftpwindowsize 4
    Write-Output "Created ramdiskoptions"
    
    # OS loader for WinPE
    $out = bcdedit /store $bcdPath /create /d "Windows PE" /application osloader
    $guid = ($out -split ' ')[2]
    Write-Output "OS loader GUID: $guid"
    
    bcdedit /store $bcdPath /set $guid device ramdisk=[boot]\Boot\boot.wim,`{ramdiskoptions`}
    bcdedit /store $bcdPath /set $guid osdevice ramdisk=[boot]\Boot\boot.wim,`{ramdiskoptions`}
    bcdedit /store $bcdPath /set $guid systemroot \Windows
    bcdedit /store $bcdPath /set $guid winpe yes
    bcdedit /store $bcdPath /set $guid detecthal yes
    bcdedit /store $bcdPath /set $guid nx optin
    Write-Output "Set OS loader"
    
    # bootmgr
    bcdedit /store $bcdPath /create `{bootmgr`} /d "Windows Boot Manager"
    bcdedit /store $bcdPath /set `{bootmgr`} device boot
    bcdedit /store $bcdPath /set `{bootmgr`} timeout 0
    bcdedit /store $bcdPath /displayorder $guid
    Write-Output "Set bootmgr"
    
    Write-Output "=== Final BCD ==="
    bcdedit /store $bcdPath /enum all
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:3000])
print("Status:", r.status_code)
