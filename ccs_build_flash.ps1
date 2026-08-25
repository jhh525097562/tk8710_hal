<#
.SYNOPSIS
Builds the CCS project, or builds and flashes it.

.EXAMPLE
  .\ccs_build_flash.ps1 build

.EXAMPLE
  .\ccs_build_flash.ps1 build-flash

.EXAMPLE
  .\ccs_build_flash.ps1 build-flash -Clean -NoRun
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'build-flash')]
    [string]$Action = 'build',

    [string]$CcsRoot = 'D:\ti\ccs1281\ccs',

    [ValidateNotNullOrEmpty()]
    [string]$Configuration = 'Debug',

    [ValidateRange(1, 64)]
    [int]$Jobs = 4,

    [switch]$Clean,

    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDirectory = Join-Path $projectRoot $Configuration
$make = Join-Path $CcsRoot 'utils\bin\gmake.exe'
$dsLite = Join-Path $CcsRoot 'ccs_base\DebugServer\bin\DSLite.exe'
$targetConfig = Join-Path $projectRoot 'targetConfigs\TMS570LS3137.ccxml'
$outputFile = Join-Path $buildDirectory 'tms570ls3137_halcogen_base_570RAM.out'

function Assert-File {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description not found: $Path"
    }
}

function Invoke-Tool {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [string[]]$Arguments,

        [Parameter(Mandatory)]
        [string]$Step
    )

    Write-Host "`n==> $Step" -ForegroundColor Cyan
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

try {
    Assert-File -Path $make -Description 'CCS gmake'
    Assert-File -Path (Join-Path $buildDirectory 'makefile') -Description 'CCS generated makefile'

    if ($Clean) {
        Invoke-Tool -Executable $make `
            -Arguments @('-C', $buildDirectory, 'clean') `
            -Step "Clean $Configuration"
    }

    Invoke-Tool -Executable $make `
        -Arguments @('-C', $buildDirectory, "-j$Jobs", 'all') `
        -Step "Build $Configuration"

    Assert-File -Path $outputFile -Description 'Build output'
    Write-Host "Build output: $outputFile" -ForegroundColor Green

    if ($Action -eq 'build-flash') {
        Assert-File -Path $dsLite -Description 'CCS DSLite'
        Assert-File -Path $targetConfig -Description 'Target configuration'

        $flashArguments = @(
            'flash'
            "--config=$targetConfig"
            '--flash'
            '--verify'
            '--verbose'
        )
        if (-not $NoRun) {
            $flashArguments += '--run'
        }
        $flashArguments += $outputFile

        Invoke-Tool -Executable $dsLite `
            -Arguments $flashArguments `
            -Step 'Flash and verify target'
        Write-Host 'Flash completed successfully.' -ForegroundColor Green
    }
}
catch {
    Write-Error $_
    exit 1
}
