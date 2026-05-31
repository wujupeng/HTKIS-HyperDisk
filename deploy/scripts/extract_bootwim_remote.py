import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Mount ISO and find boot.wim
ps = """
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$ErrorActionPreference = 'Continue'
try {
    $img = Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -ErrorAction SilentlyContinue
    if (-not $img.Attached) {
        Mount-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -StorageType ISO -Access ReadOnly | Out-Null
    }
    Start-Sleep -Seconds 5
    $vol = (Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' | Get-Volume).DriveLetter
    Write-Output "ISO mounted at drive: $vol"
    $bootWim = "${vol}:\sources\boot.wim"
    if (Test-Path $bootWim) {
        $size = (Get-Item $bootWim).Length / 1MB
        Write-Output "boot.wim found: $size MB"
        # Copy to share
        $dest = 'C:\share\bootwim'
        if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force | Out-Null }
        Write-Output "Copying boot.wim to $dest (this takes a while)..."
        Copy-Item -Path $bootWim -Destination "$dest\boot.wim" -Force
        $copied = (Get-Item "$dest\boot.wim").Length / 1MB
        Write-Output "Copied: $copied MB"
    } else {
        Write-Output "boot.wim NOT found. Listing sources:"
        Get-ChildItem "${vol}:\sources\" -Filter "*.wim" | ForEach-Object { Write-Output "  $($_.Name) - $([math]::Round($_.Length/1MB,1)) MB" }
    }
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:2000])
print("Status:", r.status_code)
