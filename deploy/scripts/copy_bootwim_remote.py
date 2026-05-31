import winrm

s = winrm.Session("192.168.2.81", auth=("9999", "1111"), transport="ntlm")

# Copy boot.wim from mounted ISO to share directory
ps = """
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try {
    # Make sure ISO is mounted
    $img = Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -ErrorAction SilentlyContinue
    if (-not $img.Attached) {
        Mount-DiskImage -ImagePath 'C:\share\installtool\win10.iso' -StorageType ISO -Access ReadOnly | Out-Null
        Start-Sleep -Seconds 5
    }
    $vol = (Get-DiskImage -ImagePath 'C:\share\installtool\win10.iso' | Get-Volume).DriveLetter
    Write-Output "Drive: $vol"
    
    $srcPath = "$vol" + ':\sources\boot.wim'
    Write-Output "Source: $srcPath"
    
    $dest = 'C:\share\bootwim'
    if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force | Out-Null }
    
    Write-Output "Copying boot.wim (796MB, will take 1-2 minutes)..."
    Copy-Item -Path $srcPath -Destination "$dest\boot.wim" -Force
    $copied = [math]::Round((Get-Item "$dest\boot.wim").Length / 1MB, 1)
    Write-Output "Copied boot.wim: $copied MB"
    
    # Also copy boot.sdi if useful
    $sdiSrc = "$vol" + ':\sources\boot.sdi'
    if (Test-Path $sdiSrc) {
        Copy-Item -Path $sdiSrc -Destination "$dest\boot.sdi" -Force
        Write-Output "Copied boot.sdi"
    }
} catch {
    Write-Output "Error: $($_.Exception.Message)"
}
"""

r = s.run_ps(ps)
out = r.std_out.decode("utf-8", errors="replace")
print("Result:", out[:2000])
print("Status:", r.status_code)
