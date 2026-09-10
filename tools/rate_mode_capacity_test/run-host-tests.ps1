[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$toolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$compiler = Get-Command gcc.exe -ErrorAction SilentlyContinue
if ($null -eq $compiler) {
    $compiler = Get-Command clang.exe -ErrorAction SilentlyContinue
}
if ($null -eq $compiler) {
    throw 'gcc.exe or clang.exe is required for the host logic test.'
}

$outputDir = Join-Path $toolRoot 'build-host'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$exe = Join-Path $outputDir 'capacity_test_logic_test.exe'

& $compiler.Source -std=c99 -Wall -Wextra -Werror `
    "-I$toolRoot" `
    (Join-Path $toolRoot 'capacity_test_logic.c') `
    (Join-Path $toolRoot 'tests\capacity_test_logic_test.c') `
    -o $exe
if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed with exit code $LASTEXITCODE."
}

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "Host test failed with exit code $LASTEXITCODE."
}
