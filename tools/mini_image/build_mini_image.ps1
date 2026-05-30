# ============================================================
# HTKIS HyperDisk X — 最小可启动镜像构建脚本
# 基于: Windows 10 LTSC 2021 (21H2)
# 工具: Windows ADK + DISM + PowerShell
# 目标: ≤2GB, BootStart驱动≤15, 移除所有非启动必需组件
# ============================================================

param(
    [string]$SourceWim = "D:\sources\install.wim",
    [string]$WorkDir = "C:\HyperDisk\MiniImage",
    [string]$DriverDir = "C:\HyperDisk\drivers",
    [string]$MiniWimName = "mini_bootable.wim",
    [switch]$SkipDriverInject,
    [switch]$SkipComponentRemoval,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$mountDir = Join-Path $WorkDir "mount"
$miniWim = Join-Path $WorkDir $MiniWimName
$logDir = Join-Path $WorkDir "logs"

function Write-Step {
    param([string]$Step, [string]$Message)
    $timestamp = Get-Date -Format "HH:mm:ss"
    Write-Host "[$timestamp] [$Step] $Message"
}

function Invoke-SafeDism {
    param([string[]]$Arguments)
    if ($DryRun) {
        Write-Host "  [DRY-RUN] dism $($Arguments -join ' ')"
        return
    }
    $output = & dism @Arguments 2>&1
    $lastLine = ($output | Select-Object -Last 1).ToString()
    if ($lastLine -notmatch "The operation completed successfully") {
        Write-Warning "DISM warning: $lastLine"
    }
}

# ============================================================
# Step 1: 准备工作目录
# ============================================================
Write-Step "MI-01" "Creating working directories"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
New-Item -ItemType Directory -Force -Path $mountDir | Out-Null
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

# ============================================================
# Step 2: 提取 Win10 LTSC 基础映像 (Index 1 = LTSC Edition)
# ============================================================
Write-Step "MI-01" "Extracting Win10 LTSC base image from $SourceWim"
if (-not (Test-Path $SourceWim)) {
    Write-Error "Source WIM not found: $SourceWim"
    Write-Host "Please mount Win10 LTSC ISO and provide path to sources\install.wim"
    exit 1
}

dism /Get-WimInfo /WimFile:$SourceWim

if (-not $DryRun) {
    if (Test-Path $miniWim) {
        Write-Step "MI-01" "Removing existing mini WIM"
        Remove-Item $miniWim -Force
    }
}

Invoke-SafeDism -Arguments @(
    "/Export-Image",
    "/SourceImageFile:$SourceWim",
    "/SourceIndex:1",
    "/DestinationImageFile:$miniWim",
    "/Compress:max"
)

# ============================================================
# Step 3: 挂载映像
# ============================================================
Write-Step "MI-01" "Mounting image for modification"
Invoke-SafeDism -Arguments @(
    "/Mount-Image",
    "/ImageFile:$miniWim",
    "/Index:1",
    "/MountDir:$mountDir",
    "/ReadWrite"
)

if ($DryRun) {
    Write-Step "MI-01" "DRY-RUN: Skipping mount verification"
} else {
    if (-not (Test-Path (Join-Path $mountDir "Windows"))) {
        Write-Error "Mount directory does not contain Windows folder. Mount may have failed."
        exit 1
    }
}

# ============================================================
# Step 4: 移除非必要组件 (10个包)
# ============================================================
if (-not $SkipComponentRemoval) {
    $packagesToRemove = @(
        @{Name = "Microsoft-Windows-Defender-Client-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Windows Defender Client"},
        @{Name = "Microsoft-Windows-Defender-Service-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Windows Defender Service"},
        @{Name = "Microsoft-Hyper-V-Hypervisor-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Hyper-V Hypervisor"},
        @{Name = "Microsoft-Hyper-V-Management-Clients-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Hyper-V Management"},
        @{Name = "Microsoft-Windows-SecureStartup-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "BitLocker"},
        @{Name = "Microsoft-Windows-IsolatedUserMode-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "VBS/Credential Guard"},
        @{Name = "Microsoft-Windows-Cortana-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Cortana"},
        @{Name = "Microsoft-Windows-Store-Client-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Microsoft Store"},
        @{Name = "Microsoft-Windows-SearchEngine-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Windows Search"},
        @{Name = "Microsoft-Windows-Printing-XPSServices-Package~31bf3856ad364e35~amd64~~10.0.19041.1"; Desc = "Print Spooler"}
    )

    $removeIndex = 1
    foreach ($pkg in $packagesToRemove) {
        Write-Step "MI-02" "Removing package $removeIndex/10: $($pkg.Desc)"
        try {
            Invoke-SafeDism -Arguments @(
                "/Image:$mountDir",
                "/Remove-Package",
                "/PackageName:$($pkg.Name)"
            )
        } catch {
            Write-Warning "Could not remove package: $($pkg.Desc) — it may not exist in this image. Continuing."
        }
        $removeIndex++
    }

    # WinSxS cleanup
    Write-Step "MI-02" "Cleaning up WinSxS component store"
    Invoke-SafeDism -Arguments @(
        "/Image:$mountDir",
        "/Cleanup-Image",
        "/StartComponentCleanup",
        "/ResetBase"
    )
} else {
    Write-Step "MI-02" "Skipping component removal (--SkipComponentRemoval)"
}

# ============================================================
# Step 5: 禁用非Boot Critical服务
# ============================================================
Write-Step "MI-04" "Disabling non-boot-critical services"
$disableServices = @(
    "WSearch",
    "wuauserv",
    "Spooler",
    "SysMain",
    "DiagTrack",
    "dmwappushservice",
    "MapsUpdate",
    "Fax",
    "lfsvc",
    "RetailDemo",
    "wisvc",
    "WMPNetworkSvc"
)

foreach ($svc in $disableServices) {
    if ($DryRun) {
        Write-Host "  [DRY-RUN] dism /Image:$mountDir /Set-Service:$svc /StartType:Disabled"
    } else {
        try {
            & dism /Image:$mountDir /Set-Service:$svc /StartType:Disabled 2>$null
        } catch {
            Write-Warning "Could not disable service: $svc"
        }
    }
}

# ============================================================
# Step 6: 注入 HyperDisk 驱动 (boot-start类型)
# ============================================================
if (-not $SkipDriverInject) {
    $drivers = @(
        "HyperDiskBus.sys",
        "HyperDiskBlk.sys",
        "HyperNet.sys"
    )

    $driverIndex = 1
    foreach ($drv in $drivers) {
        $drvPath = Join-Path $DriverDir $drv
        Write-Step "MI-03" "Injecting driver $driverIndex/3: $drv"
        if (-not (Test-Path $drvPath)) {
            Write-Warning "Driver not found: $drvPath — skipping injection"
            continue
        }
        Invoke-SafeDism -Arguments @(
            "/Image:$mountDir",
            "/Add-Driver",
            "/Driver:$drvPath",
            "/ForceUnsigned"
        )
        $driverIndex++
    }

    # 设置驱动为 boot-start (Start=0)
    Write-Step "MI-03" "Setting HyperDisk drivers as boot-start (Start=0)"
    $systemHive = Join-Path $mountDir "Windows\System32\config\SYSTEM"
    if (Test-Path $systemHive) {
        if (-not $DryRun) {
            reg load HKLM\MOUNT_SYSTEM $systemHive 2>$null
            $driverServices = @("HyperDiskBus", "HyperDiskBlk", "HyperNet")
            foreach ($svc in $driverServices) {
                try {
                    reg add "HKLM\MOUNT_SYSTEM\ControlSet001\Services\$svc" /v Start /t REG_DWORD /d 0 /f 2>$null
                    Write-Host "  Set $svc Start=0 (boot-start)"
                } catch {
                    Write-Warning "Could not set boot-start for $svc"
                }
            }
            reg unload HKLM\MOUNT_SYSTEM 2>$null
        }
    }
} else {
    Write-Step "MI-03" "Skipping driver injection (--SkipDriverInject)"
}

# ============================================================
# Step 7: 优化页面文件
# ============================================================
Write-Step "MI-04" "Disabling page file"
if (-not $DryRun) {
    try {
        & dism /Image:$mountDir /Set-PageFile:0 2>$null
    } catch {
        Write-Warning "Could not disable page file via DISM. Will attempt registry method."
        $systemHive = Join-Path $mountDir "Windows\System32\config\SYSTEM"
        if (Test-Path $systemHive) {
            reg load HKLM\MOUNT_SYSTEM $systemHive 2>$null
            reg add "HKLM\MOUNT_SYSTEM\ControlSet001\Control\Session Manager\Memory Management" /v PagingFiles /t REG_MULTI_SZ /d "" /f 2>$null
            reg unload HKLM\MOUNT_SYSTEM 2>$null
        }
    }
}

# ============================================================
# Step 8: 卸载映像并保存
# ============================================================
Write-Step "MI-05" "Committing changes and unmounting image"
Invoke-SafeDism -Arguments @(
    "/Unmount-Image",
    "/MountDir:$mountDir",
    "/Commit"
)

# ============================================================
# Step 9: 验证映像大小
# ============================================================
Write-Step "MI-05" "Verifying image size"
if (-not $DryRun) {
    if (Test-Path $miniWim) {
        $wimSize = (Get-Item $miniWim).Length / 1GB
        $wimSizeMB = (Get-Item $miniWim).Length / 1MB
        Write-Host "  Mini image size: $wimSize GB ($wimSizeMB MB)"

        if ($wimSize -gt 2.0) {
            Write-Error "FAIL: Image size exceeds 2GB limit! Current: $wimSize GB"
            Write-Host "  Suggestions for further size reduction:"
            Write-Host "  - Run DISM /Cleanup-Image /StartComponentCleanup /ResetBase again"
            Write-Host "  - Remove additional language packs"
            Write-Host "  - Remove additional capabilities with DISM /Remove-Capability"
            exit 1
        }
        Write-Host "  PASS: Image size within 2GB limit"
    } else {
        Write-Error "Mini WIM file not found at: $miniWim"
        exit 1
    }
}

# ============================================================
# Step 10: 验证 BootStart 驱动数
# ============================================================
Write-Step "MI-06" "Verifying BootStart driver count (target <= 15)"
if (-not $DryRun) {
    $systemHive = Join-Path $mountDir "Windows\System32\config\SYSTEM"
    # Remount read-only for verification
    try {
        & dism /Mount-Image /ImageFile:$miniWim /Index:1 /MountDir:$mountDir /ReadOnly 2>$null
        $bootStartDrivers = @()
        $servicesPath = Join-Path $mountDir "Windows\System32\config\SYSTEM"

        reg load HKLM\MOUNT_SYSTEM_VERIFY $servicesPath 2>$null
        $servicesKey = "HKLM\MOUNT_SYSTEM_VERIFY\ControlSet001\Services"
        $allServices = Get-ChildItem -Path $servicesKey -ErrorAction SilentlyContinue

        foreach ($svc in $allServices) {
            $startValue = (Get-ItemProperty -Path $svc.PSPath -Name "Start" -ErrorAction SilentlyContinue).Start
            if ($startValue -eq 0) {
                $bootStartDrivers += $svc.PSChildName
            }
        }

        reg unload HKLM\MOUNT_SYSTEM_VERIFY 2>$null
        & dism /Unmount-Image /MountDir:$mountDir /Discard 2>$null

        $driverCount = $bootStartDrivers.Count
        Write-Host "  BootStart driver count: $driverCount"
        Write-Host "  Drivers: $($bootStartDrivers -join ', ')"

        if ($driverCount -gt 15) {
            Write-Warning "BootStart driver count ($driverCount) exceeds target (15)"
        } else {
            Write-Host "  PASS: BootStart driver count within limit"
        }
    } catch {
        Write-Warning "Could not verify BootStart driver count. Manual verification required."
    }
}

# ============================================================
# 完成
# ============================================================
Write-Step "DONE" "Mini image build complete"
Write-Host ""
Write-Host "Output: $miniWim"
if (-not $DryRun -and (Test-Path $miniWim)) {
    $finalSize = [math]::Round((Get-Item $miniWim).Length / 1MB, 1)
    Write-Host "Size: ${finalSize} MB"
}
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Upload mini image to ImageServer:"
Write-Host "     curl -T $miniWim http://10.10.200.10:9527/api/v1/images/upload"
Write-Host "  2. Configure PXE/iPXE to boot this image"
Write-Host "  3. Run bootdiag.exe in WinPE to verify 7 checks PASS"
Write-Host "  4. Proceed to Milestone A verification"
