$ErrorActionPreference = 'Stop'

$toolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $toolDir 'combined_spi_gui_tester.c'
$exe = Join-Path $toolDir 'combined_spi_gui_tester.exe'
$jtoolInclude = Join-Path (Split-Path -Parent $toolDir) 'fpga_jtool_tester'
$jtoolDll = Join-Path $jtoolInclude 'jtool.dll'

if (-not (Test-Path -LiteralPath $source)) {
    throw "Source not found: $source"
}
if (-not (Test-Path -LiteralPath (Join-Path $jtoolInclude 'jtool.h'))) {
    throw "jtool.h not found: $jtoolInclude"
}
if (-not (Test-Path -LiteralPath $jtoolDll)) {
    throw "jtool.dll not found: $jtoolDll"
}

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if ($null -eq $gcc) {
    throw 'gcc was not found in PATH. Install MinGW-w64 or add gcc.exe to PATH.'
}

& $gcc.Source -Wall -Wextra -std=c99 -O2 -I$toolDir -I$jtoolInclude $source -o $exe -mwindows -lcomctl32 -lwinmm
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Copy-Item -LiteralPath $jtoolDll -Destination (Join-Path $toolDir 'jtool.dll') -Force

Write-Host "Built: $exe"
