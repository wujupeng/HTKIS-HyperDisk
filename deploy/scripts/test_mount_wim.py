import winrm, time

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Customize boot.wim: inject hd_boot_agent.exe + configure startnet.cmd
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$ErrorActionPreference = 'Continue'

try {
    $wimPath = 'C:\bootwim_temp\boot.wim'
    $mountDir = 'C:\bootwim_temp\mount'
    $agentSrc = 'C:\share\installtool'  # We'll upload agent here first
    
    # Create mount dir
    if (-not (Test-Path $mountDir)) { New-Item -ItemType Directory -Path $mountDir -Force | Out-Null }
    
    # Mount boot.wim (index 2 = WinPE)
    Write-Output "Mounting boot.wim (index 2)..."
    dism /Mount-Wim /WimFile:$wimPath /Index:2 /MountDir:$mountDir
    Start-Sleep -Seconds 3
    
    # Verify mount
    if (Test-Path "$mountDir\Windows") {
        Write-Output "Mounted successfully"
        
        # Check existing startnet.cmd
        $startnet = "$mountDir\Windows\System32\startnet.cmd"
        if (Test-Path $startnet) {
            Write-Output "Current startnet.cmd:"
            Get-Content $startnet | Select-Object -First 5
        }
        
        # List Windows dir
        Write-Output "Windows dir exists: $(Test-Path $mountDir\Windows)"
        Write-Output "System32 dir exists: $(Test-Path $mountDir\Windows\System32)"
        
    } else {
        Write-Output "Mount failed - Windows dir not found"
        dism /Unmount-Wim /MountDir:$mountDir /Discard
        return
    }
    
    # Unmount (we'll do the real customization after uploading the agent)
    Write-Output "Unmounting (discard - just testing)..."
    dism /Unmount-Wim /MountDir:$mountDir /Discard
    Write-Output "Done - mount/unmount test successful"
    
} catch {
    Write-Output "Error: $($_.Exception.Message)"
    dism /Unmount-Wim /MountDir:$mountDir /Discard -ErrorAction SilentlyContinue
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:2000])
print("Status:", r.status_code)
