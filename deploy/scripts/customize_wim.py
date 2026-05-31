import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    # Remove read-only attribute
    $wimPath = 'C:\bootwim_temp\boot.wim'
    attrib -R $wimPath
    Write-Output "Removed readonly attribute"
    
    $mountDir = 'C:\bootwim_temp\mount'
    if (-not (Test-Path $mountDir)) { New-Item -ItemType Directory -Path $mountDir -Force | Out-Null }
    
    # Mount with index 2 (WinPE)
    Write-Output "Mounting boot.wim index 2..."
    $result = dism /Mount-Wim /WimFile:$wimPath /Index:2 /MountDir:$mountDir 2>&1
    Write-Output ($result | Out-String)
    
    Start-Sleep -Seconds 5
    
    if (Test-Path "$mountDir\Windows\System32") {
        Write-Output "MOUNT OK"
        
        # Copy hd_boot_agent.exe
        $agent = 'C:\share\installtool\hd_boot_agent.exe'
        if (Test-Path $agent) {
            Copy-Item $agent "$mountDir\hd_boot_agent.exe" -Force
            Write-Output "Copied hd_boot_agent.exe"
        } else {
            Write-Output "Agent not found at $agent"
        }
        
        # Write startnet.cmd
        $startnet = "$mountDir\Windows\System32\startnet.cmd"
        @"
@echo off
wpeinit
echo ============================================
echo HTKIS HyperDisk X - BootAgent Starting...
echo ============================================
X:\hd_boot_agent.exe server=192.168.2.80 port=9527 meta=http://192.168.2.80/boot/boot.meta
echo BootAgent exited with error level %ERRORLEVEL%
echo Press any key to restart...
pause
"@ | Set-Content $startnet -Force
        Write-Output "Written startnet.cmd"
        
        # Commit changes
        Write-Output "Committing changes..."
        $commit = dism /Unmount-Wim /MountDir:$mountDir /Commit 2>&1
        Write-Output ($commit | Out-String)
        Write-OUTPUT "DONE"
    } else {
        Write-Output "Mount failed"
        dism /Unmount-Wim /MountDir:$mountDir /Discard 2>&1 | Out-Null
    }
} catch {
    Write-Output "Error: $($_.Exception.Message)"
    dism /Unmount-Wim /MountDir:$mountDir /Discard 2>&1 | Out-Null
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:4000])
print("Status:", r.status_code)
