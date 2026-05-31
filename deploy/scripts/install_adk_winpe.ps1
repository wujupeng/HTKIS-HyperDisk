$ErrorActionPreference = 'Stop'
Write-Output "=== HTKIS HyperDisk X - Full ADK + WinPE Installation ==="

# Step 1: Install full ADK 26100
$adkUrl = 'https://go.microsoft.com/fwlink/?linkid=2289980'
$adkOut = "$env:TEMP\adksetup.exe"
Write-Output "Step 1: Downloading ADK 26100..."
Invoke-WebRequest -Uri $adkUrl -OutFile $adkOut
Write-Output "ADK installer: $((Get-Item $adkOut).Length) bytes"
Write-Output "Installing ADK (Deployment Tools + WinPE)..."
$proc = Start-Process -FilePath $adkOut -ArgumentList "/quiet", "/features", "OptionId.DeploymentTools", "/features", "OptionId.WinPE" -Wait -PassThru
Write-Output "ADK exit code: $($proc.ExitCode)"

# If WinPE not installed via ADK, try the addon
$winpePath = "C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit\winpe"
if (-not (Test-Path $winpePath)) {
    Write-Output "WinPE not in ADK, installing WinPE addon separately..."
    $winpeUrl = 'https://go.microsoft.com/fwlink/?linkid=2289981'
    $winpeOut = "$env:TEMP\adkwinpesetup.exe"
    Invoke-WebRequest -Uri $winpeUrl -OutFile $winpeOut
    Write-Output "WinPE addon: $((Get-Item $winpeOut).Length) bytes"
    $proc2 = Start-Process -FilePath $winpeOut -ArgumentList "/quiet", "/features", "OptionId.WinPE" -Wait -PassThru
    Write-Output "WinPE addon exit code: $($proc2.ExitCode)"
}

# Verify
if (Test-Path $winpePath) {
    Write-Output "SUCCESS: WinPE installed!"
    Get-ChildItem $winpePath -Directory
    $copype = "C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit\Deployment Tools\amd64\DISM\dism.exe"
    if (Test-Path $copype) { Write-Output "DISM found: $copype" }
} else {
    Write-Output "WARNING: WinPE not found at expected path"
    Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit" -Directory -Recurse -Depth 1 -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "winpe|WinPE" }
}
