[CmdletBinding()]
param(
    [string]$WslDistro = 'Ubuntu-22.04',
    [string]$SdkRoot = '/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0',
    [string]$OutputImage = 'D:\RK3506_Flash\rk3506b_iot_emmc_update.img',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backupDir = Join-Path $scriptDir "backup\$timestamp"
$sourceFiles = [ordered]@{
    'tk8710_gw'  = 'D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\tk8710app\tk8710_gw'
    'web_server'  = 'D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\webserver\build_arm\web_server'
    'mqtt_bridge' = 'D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\mqtt\build_arm\mqtt_bridge'
}
$startupCommand = 'proc=/userdata/tk8710_gw -w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level warn'

function Convert-ToWslPath([string]$WindowsPath) {
    $full = [System.IO.Path]::GetFullPath($WindowsPath)
    if ($full -notmatch '^([A-Za-z]):\\(.*)$') {
        throw "无法转换为 WSL 路径：$WindowsPath"
    }
    $drive = $Matches[1].ToLowerInvariant()
    $tail = $Matches[2] -replace '\\', '/'
    return "/mnt/$drive/$tail"
}

function Invoke-WslBash([string]$Command) {
    & wsl.exe -d $WslDistro -- bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "WSL 命令失败，退出码 $LASTEXITCODE：$Command"
    }
}

$distroNames = (& wsl.exe --list --quiet) -replace "`0", '' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
if ($WslDistro -notin $distroNames) {
    throw "未找到 WSL 发行版 '$WslDistro'。现有发行版：$($distroNames -join ', ')"
}

foreach ($entry in $sourceFiles.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "缺少 $($entry.Key) 源文件：$($entry.Value)"
    }
}

$sdkUncRoot = "\\wsl.localhost\$WslDistro" + ($SdkRoot -replace '/', '\')
$userdataDir = Join-Path $sdkUncRoot 'device\rockchip\common\extra-parts\userdata\normal'
$tdaemonConfig = Join-Path $sdkUncRoot 'buildroot\board\vanxoak\hd_rk3506_iot_emmc\fs-overlay\etc\tdaemon.conf'
if (-not (Test-Path -LiteralPath $userdataDir -PathType Container)) {
    throw "SDK userdata 预置目录不存在：$userdataDir"
}
if (-not (Test-Path -LiteralPath $tdaemonConfig -PathType Leaf)) {
    throw "tdaemon 配置不存在：$tdaemonConfig"
}

Write-Host '检查 ARM ELF 文件...'
foreach ($entry in $sourceFiles.GetEnumerator()) {
    $wslSource = Convert-ToWslPath $entry.Value
    $fileInfo = (& wsl.exe -d $WslDistro -- file -b $wslSource) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $fileInfo -notmatch 'ELF.*ARM') {
        throw "$($entry.Key) 不是有效的 ARM ELF：$fileInfo"
    }
    Write-Host "  $($entry.Key): $fileInfo"
}

New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
foreach ($name in $sourceFiles.Keys) {
    $oldFile = Join-Path $userdataDir $name
    if (Test-Path -LiteralPath $oldFile -PathType Leaf) {
        Copy-Item -LiteralPath $oldFile -Destination (Join-Path $backupDir $name)
    }
}
Copy-Item -LiteralPath $tdaemonConfig -Destination (Join-Path $backupDir 'tdaemon.conf')

Write-Host "备份目录：$backupDir"
foreach ($entry in $sourceFiles.GetEnumerator()) {
    $destination = Join-Path $userdataDir $entry.Key
    Copy-Item -LiteralPath $entry.Value -Destination $destination -Force
    Write-Host "已更新：$destination"
}

$configText = [System.IO.File]::ReadAllText($tdaemonConfig)
if ($configText -notmatch '(?m)^proc=/userdata/tk8710_gw(?:\s.*)?$') {
    throw 'tdaemon.conf 中未找到 proc=/userdata/tk8710_gw 配置行，未自动插入以避免误改。'
}
$configText = [regex]::Replace($configText, '(?m)^proc=/userdata/tk8710_gw(?:\s.*)?$', $startupCommand)
[System.IO.File]::WriteAllText($tdaemonConfig, $configText, [System.Text.UTF8Encoding]::new($false))
Write-Host "已更新守护命令：$startupCommand"

Write-Host 'SDK 预置文件 SHA-256：'
foreach ($name in $sourceFiles.Keys) {
    Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $userdataDir $name) |
        Select-Object Path, Hash | Format-Table -AutoSize
}

if ($SkipBuild) {
    Write-Warning '已按 -SkipBuild 跳过 SDK 构建和升级包复制。'
    exit 0
}

if ($SdkRoot.Contains("'")) {
    throw "SDK 路径不能包含单引号：$SdkRoot"
}
$quotedSdk = "'$SdkRoot'"
Invoke-WslBash "cd $quotedSdk && ./build.sh buildroot"
Invoke-WslBash "cd $quotedSdk && ./build.sh extra-parts"
Invoke-WslBash "cd $quotedSdk && ./build.sh updateimg"

$preferredImage = Join-Path $sdkUncRoot 'output\update\Image\update.img'
if ((Test-Path -LiteralPath $preferredImage -PathType Leaf) -and
    ((Get-Item -LiteralPath $preferredImage).Length -gt 1MB)) {
    $sdkImage = $preferredImage
} else {
    # output/update.img、output/update/update.img 和 output/firmware/update.img
    # 在 Linux 中可能是符号链接，经由 \\wsl.localhost 访问时会表现为十几字节的小文件。
    # 兜底时只接受大于 1 MiB 的非 raw 完整升级镜像。
    $sdkImage = Get-ChildItem -LiteralPath (Join-Path $sdkUncRoot 'output') -Recurse -File -Filter 'update.img' |
        Where-Object { $_.Length -gt 1MB -and $_.Name -ne 'update.raw.img' } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $sdkImage) {
    throw "构建结束但未找到大于 1 MiB 的完整升级包，已检查：$preferredImage"
}
Write-Host "找到完整升级包：$sdkImage ($((Get-Item -LiteralPath $sdkImage).Length) bytes)"

$outputDir = Split-Path -Parent $OutputImage
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
if (Test-Path -LiteralPath $OutputImage -PathType Leaf) {
    Copy-Item -LiteralPath $OutputImage -Destination (Join-Path $backupDir 'rk3506b_iot_emmc_update.img')
}
Copy-Item -LiteralPath $sdkImage -Destination $OutputImage -Force

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdkImage).Hash
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputImage).Hash
if ($sourceHash -ne $outputHash) {
    throw '升级包复制后的 SHA-256 与 SDK 产物不一致。'
}

Write-Host '升级包制作完成。' -ForegroundColor Green
Write-Host "SDK 产物：$sdkImage"
Write-Host "目标文件：$OutputImage"
Write-Host "SHA-256：$outputHash"
