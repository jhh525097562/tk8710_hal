$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $root 'bridge\jtool_spi_bridge.c'
$exe = Join-Path $root 'bridge\jtool_spi_bridge.exe'
$dllCandidates = @(
    (Join-Path $root 'bridge\jtool.dll'),
    'D:\CodexWorkSpace\卫星物联网演示方案\载荷软件测试\combined_spi_gui_tester\jtool.dll',
    (Join-Path (Split-Path -Parent $root) 'jtool.dll')
)
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if ($null -eq $gcc) { throw 'gcc.exe not found. Install MinGW-w64 or add it to PATH.' }

& $gcc.Source -Wall -Wextra -std=c99 -O2 $source -o $exe
if ($LASTEXITCODE -ne 0) { throw "SPI bridge build failed: $LASTEXITCODE" }

$dll = $dllCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $dll) { throw 'jtool.dll not found.' }
if ((Resolve-Path -LiteralPath $dll).Path -ne (Join-Path $root 'bridge\jtool.dll')) {
    Copy-Item -LiteralPath $dll -Destination (Join-Path $root 'bridge\jtool.dll') -Force
}
Write-Host "Built: $exe"
