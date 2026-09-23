[CmdletBinding()]
param(
    [switch]$Init,
    [switch]$Apply,
    [string[]]$Select,
    [string[]]$Resolve,
    [switch]$AllowDelete,
    [string]$Distribution = 'Ubuntu-22.04',
    [string]$AppRoot = 'D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app',
    [string]$SdkRoot = '/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0'
)
$ErrorActionPreference = 'Stop'
if ($Init -and $Apply) { throw '-Init 和 -Apply 不能同时使用' }
function Convert-ToWslPath([string]$Path) {
    if ($Path.StartsWith('/')) { return $Path }
    # Forward slashes avoid native argument parsing consuming backslashes.
    $converted = & wsl.exe -d $Distribution --exec wslpath -u $Path.Replace('\', '/')
    if ($LASTEXITCODE -ne 0) { throw "无法转换路径：$Path" }
    return ($converted -join "`n").Trim()
}
$repoPath = Split-Path -Parent $PSScriptRoot
$linuxRepo = Convert-ToWslPath $repoPath
$linuxApp = Convert-ToWslPath $AppRoot
$linuxSdk = Convert-ToWslPath $SdkRoot
$arguments = @('-d', $Distribution, '--exec', 'env', 'PYTHONUTF8=1', 'python3',
    "$linuxRepo/scripts/sync_sources.py", '--repo-root', $linuxRepo,
    '--app-root', $linuxApp, '--sdk-root', $linuxSdk)
if ($Init) { $arguments += '--init' }
if ($Apply) { $arguments += '--apply' }
if ($AllowDelete) { $arguments += '--allow-delete' }
foreach ($entry in $Select) { $arguments += @('--select', $entry) }
foreach ($entry in $Resolve) { $arguments += @('--resolve', $entry) }
& wsl.exe @arguments
exit $LASTEXITCODE
