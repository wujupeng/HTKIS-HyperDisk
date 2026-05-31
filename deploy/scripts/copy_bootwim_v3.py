import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Create dir with proper permissions, then copy
ps = r"""
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    # Create dest dir on C: (not in share) with Everyone full access
    $dest = 'C:\bootwim_temp'
    if (-not (Test-Path $dest)) { 
        New-Item -ItemType Directory -Path $dest -Force | Out-Null
        icacls $dest /grant "Everyone:(OI)(CI)F" /T | Out-Null
    }
    Write-Output "Dest: $dest"
    
    # Make sure ISO is mounted
    $img = Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -ErrorAction SilentlyContinue
    if (-not $img.Attached) {
        Mount-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -StorageType ISO -Access ReadOnly | Out-Null
        Start-Sleep -Seconds 5
    }
    $vol = (Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' | Get-Volume).DriveLetter
    Write-Output "Drive: $vol"
    
    $srcPath = "${vol}:\sources\boot.wim"
    Write-Output "Copying boot.wim to $dest (800MB, ~2min)..."
    Copy-Item -LiteralPath $srcPath -Destination "$dest\boot.wim" -Force
    $f = Get-Item "$dest\boot.wim"
    $copied = [math]::Round($f.Length / 1MB, 1)
    Write-Output "Copied: $copied MB"
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:2000])
print("Status:", r.status_code)
