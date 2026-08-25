param(
    [string]$JtoolDir = (Join-Path $PSScriptRoot '..\rk3506_8710\port\jtool\x64')
)

$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'jtool_spi2_dma_test.c'
$output = Join-Path $PSScriptRoot 'jtool_spi2_dma_test.exe'
$header = Join-Path $JtoolDir 'jtool.h'
$dll = Join-Path $JtoolDir 'jtool.dll'
$gcc = (Get-Command gcc -ErrorAction Stop).Source

foreach ($path in @($source, $header, $dll)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required file was not found: $path"
    }
}

& $gcc -Wall -Wextra -std=c99 -O2 "-I$JtoolDir" $source -o $output
if ($LASTEXITCODE -ne 0) {
    throw "JTool SPI2 DMA tester build failed with exit code $LASTEXITCODE."
}
Copy-Item -LiteralPath $dll -Destination (Join-Path $PSScriptRoot 'jtool.dll') -Force
Write-Host "Built: $output"
