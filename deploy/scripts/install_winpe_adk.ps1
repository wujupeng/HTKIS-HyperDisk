$ErrorActionPreference = 'Stop'
Write-Output "=== HTKIS HyperDisk X - WinPE ADK Addon Installation ==="

# WinPE addon for ADK 10.1.26100.2454 (December 2024)
$url = 'https://go.microsoft.com/fwlink/?linkid=2289981'
$out = "$env:TEMP\adkwinpesetup.exe"

Write-Output "Downloading WinPE addon (ADK 26100)..."
Write-Output "URL: $url"
Invoke-WebRequest -Uri $url -OutFile $out
$size = (Get-Item $out).Length
Write-Output "Downloaded: $size bytes"

Write-Output "Installing WinPE addon (this may take several minutes)..."
$proc = Start-Process -FilePath $out -ArgumentList "/quiet", "/features", "OptionId.WinPE" -Wait -PassThru
Write-Output "Exit code: $($proc.ExitCode)"

# Verify installation
$winpePath = "C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit\winpe"
if (Test-Path $winpePath) {
    Write-Output "SUCCESS: WinPE addon installed!"
    Get-ChildItem $winpePath -Directory
} else {
    Write-Output "Checking alternate locations..."
    $adkPath = "C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit"
    Get-ChildItem $adkPath -Directory -ErrorAction SilentlyContinue
}
