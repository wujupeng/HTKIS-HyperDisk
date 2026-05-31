import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Create a proper BCD for wimboot RAM disk boot
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    $bcdPath = 'C:\bootwim_temp\BCD'
    
    # Remove existing BCD
    if (Test-Path $bcdPath) { Remove-Item $bcdPath -Force }
    
    # Create new BCD store
    bcdedit /createstore $bcdPath
    Write-Output "Created BCD store"
    
    # Create {ramdiskoptions} 
    bcdedit /store $bcdPath /create `{ramdiskoptions`} /d `"Ramdisk Options`"
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisksdidevice boot
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisksdipath \boot\boot.sdi
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdisktftpblocksize 4096
    bcdedit /store $bcdPath /set `{ramdiskoptions`} ramdiskmaxtftpblocksize 4096
    Write-Output "Created ramdiskoptions"
    
    # Create WinPE OS loader entry
    $output = bcdedit /store $bcdPath /create /d `"Windows PE`" /application osloader
    Write-Output "Create output: $output"
    $guid = ($output -split ' ')[2]
    Write-Output "GUID: $guid"
    
    # Set OS loader properties
    bcdedit /store $bcdPath /set $guid device ramdisk=[boot]\boot\boot.wim,`{ramdiskoptions`}
    bcdedit /store $bcdPath /set $guid osdevice ramdisk=[boot]\boot\boot.wim,`{ramdiskoptions`}
    bcdedit /store $bcdPath /set $guid systemroot \Windows
    bcdedit /store $bcdPath /set $guid winpe yes
    bcdedit /store $bcdPath /set $guid detecthal yes
    bcdedit /store $bcdPath /set $guid nx optin
    bcdedit /store $bcdPath /set $guid pae Default
    Write-Output "Set OS loader properties"
    
    # Create bootmgr entry
    bcdedit /store $bcdPath /create `{bootmgr`} /d `"Windows Boot Manager`"
    bcdedit /store $bcdPath /set `{bootmgr`} device boot
    bcdedit /store $bcdPath /set `{bootmgr`} timeout 0
    bcdedit /store $bcdPath /displayorder $guid
    Write-Output "Set bootmgr"
    
    # Verify
    $size = (Get-Item $bcdPath).Length
    Write-Output "BCD created: $size bytes"
    Write-Output "=== BCD contents ==="
    bcdedit /store $bcdPath /enum all
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:3000])
print("Status:", r.status_code)
