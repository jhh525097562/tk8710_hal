param(
    [ValidateSet('Combined', 'Boot', 'App')]
    [string]$Image = 'Combined',

    [switch]$ConnectionCheck,

    [switch]$VerifyOnly,

    [switch]$NoRun,

    [switch]$SkipBuild,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [switch]$FpgaSelfTest,

    [switch]$DataTransferSpi2SlaveTest,

    [string]$CcsRoot,

    [string]$CgtRoot,

    [string]$F021Root,

    [string]$UniFlashRoot,

    [string]$Config
)

$ErrorActionPreference = 'Stop'

function Find-FirstPath {
    param([string[]]$Paths)

    foreach ($path in $Paths) {
        if ((-not [string]::IsNullOrWhiteSpace($path)) -and (Test-Path -LiteralPath $path)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $null
}

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

$dsliteCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($UniFlashRoot)) {
    $dsliteCandidates += @(
        (Join-Path $UniFlashRoot 'dslite.bat'),
        (Join-Path $UniFlashRoot 'DSLite.exe'),
        (Join-Path $UniFlashRoot 'ccs_base\DebugServer\bin\DSLite.exe')
    )
}
$dsliteCandidates += @(
    'F:\ti\uniflash_9.6.0\dslite.bat',
    'D:\ti\uniflash_9.6.0\dslite.bat',
    'C:\ti\uniflash_9.6.0\dslite.bat',
    'F:\ti\ccs1281\ccs\ccs_base\DebugServer\bin\DSLite.exe',
    'D:\ti\ccs1281\ccs\ccs_base\DebugServer\bin\DSLite.exe',
    'C:\ti\ccs1281\ccs\ccs_base\DebugServer\bin\DSLite.exe'
)
$dslite = Find-FirstPath $dsliteCandidates
if ([string]::IsNullOrWhiteSpace($dslite)) {
    throw 'DSLite was not found. Pass -UniFlashRoot <path-to-uniflash-or-ccs>.'
}
$isStandaloneUniFlash = ([System.IO.Path]::GetExtension($dslite) -ieq '.bat')

if ([string]::IsNullOrWhiteSpace($Config)) {
    $Config = Join-Path $projectRoot 'targetConfigs\TMS570LS3137.ccxml'
} elseif (-not [System.IO.Path]::IsPathRooted($Config)) {
    $Config = Join-Path $projectRoot $Config
}

if (-not (Test-Path -LiteralPath $Config)) {
    throw "Target config was not found: $Config"
}

$imagePath = switch ($Image) {
    'Combined' { Join-Path $projectRoot 'combined\tms570_boot_app_flash.hex' }
    'Boot' { Join-Path $projectRoot 'bootloader\Debug\tms570ls3137_bootloader.out' }
    'App' { Join-Path $projectRoot 'Debug\tms570ls3137_halcogen_base_570RAM.out' }
}

Write-Host "Project: $projectRoot"
Write-Host "DSLite: $dslite"
Write-Host "Config: $Config"
if ($FpgaSelfTest) {
    Write-Warning 'FpgaSelfTest builds an FPGA-protocol-only image: TK8710 SPI bring-up and TK8710 IRQ/runtime processing are disabled.'
}

if ($ConnectionCheck) {
    Write-Host 'Checking target connection only.'
    if ($isStandaloneUniFlash) {
        & $dslite --config=$Config --list-cores
    } else {
        & $dslite flash --config=$Config --list-cores
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Target connection check failed with exit code $LASTEXITCODE."
    }
    return
}

if ((-not $VerifyOnly) -and (-not $SkipBuild)) {
    $bootBuildScript = Join-Path $PSScriptRoot 'build-bootloader.ps1'
    $appBuildScript = Join-Path $PSScriptRoot 'build-tms570.ps1'
    $combinedBuildScript = Join-Path $PSScriptRoot 'build-combined-image.ps1'

    if (($Image -eq 'Combined') -or ($Image -eq 'Boot')) {
        Write-Host 'Step 1: building Bootloader.'
        $bootArgs = @{}
        if (-not [string]::IsNullOrWhiteSpace($CgtRoot)) { $bootArgs.CgtRoot = $CgtRoot }
        if (-not [string]::IsNullOrWhiteSpace($F021Root)) { $bootArgs.F021Root = $F021Root }
        & $bootBuildScript @bootArgs
        if ($LASTEXITCODE -ne 0) { throw "Bootloader build failed with exit code $LASTEXITCODE." }
    }

    if (($Image -eq 'Combined') -or ($Image -eq 'App')) {
        Write-Host 'Step 2: building application.'
        if ($FpgaSelfTest) {
            Write-Host 'Application profile: FPGA protocol self-test; TK8710 hardware disabled.'
        } elseif ($DataTransferSpi2SlaveTest) {
            Write-Host 'Application profile: TK8710 enabled; SPI2 data-transfer slave test enabled.'
        } else {
            Write-Host 'Application profile: normal TK8710 application.'
        }
        $appArgs = @{ Configuration = $Configuration }
        if ($FpgaSelfTest) { $appArgs.FpgaSelfTest = $true }
        if ($DataTransferSpi2SlaveTest) { $appArgs.DataTransferSpi2SlaveTest = $true }
        if (-not [string]::IsNullOrWhiteSpace($CcsRoot)) { $appArgs.CcsRoot = $CcsRoot }
        if (-not [string]::IsNullOrWhiteSpace($CgtRoot)) { $appArgs.CgtRoot = $CgtRoot }
        if (-not [string]::IsNullOrWhiteSpace($F021Root)) { $appArgs.F021Root = $F021Root }
        & $appBuildScript @appArgs
        if ($LASTEXITCODE -ne 0) { throw "Application build failed with exit code $LASTEXITCODE." }
    }

    if ($Image -eq 'Combined') {
        Write-Host 'Step 3: building combined image.'
        $combinedArgs = @{}
        if (-not [string]::IsNullOrWhiteSpace($CgtRoot)) { $combinedArgs.CgtRoot = $CgtRoot }
        & $combinedBuildScript @combinedArgs
        if ($LASTEXITCODE -ne 0) { throw "Combined image build failed with exit code $LASTEXITCODE." }
    }
}

if (-not (Test-Path -LiteralPath $imagePath)) {
    throw "Image was not found: $imagePath"
}

Write-Host "Image: $imagePath"
Write-Host 'Step 4: flashing and verifying image.'
if ($isStandaloneUniFlash) {
    # Standalone UniFlash dslite.bat uses the legacy form without a leading
    # operation: dslite.bat --config=... --flash image --verify --verbose.
    $flashArgs = @(
        "--config=$Config",
        '--setting=FlashEraseSelection=1'
    )
    if ($VerifyOnly) {
        $flashArgs += @('--verify', $imagePath)
    } else {
        $flashArgs += @('--flash', $imagePath, '--verify')
        if (-not $NoRun) { $flashArgs += '--run' }
    }
    $flashArgs += '--verbose'
} else {
    # CCS DebugServer DSLite.exe requires the explicit flash operation.
    $flashArgs = @(
        'flash',
        "--config=$Config",
        '--setting=FlashEraseSelection=1',
        '--verify',
        '--verbose'
    )
    if (-not $VerifyOnly) {
        $flashArgs += '--flash'
        if (-not $NoRun) { $flashArgs += '--run' }
    }
    $flashArgs += $imagePath
}

if ($VerifyOnly) {
    Write-Host 'Verifying target flash without programming.'
} else {
    if ($NoRun) {
        Write-Host 'Programming only the sectors required by the selected image, then verifying without running.'
    } else {
        Write-Host 'Programming only the sectors required by the selected image, verifying, then running.'
    }
}

& $dslite @flashArgs
if ($LASTEXITCODE -ne 0) {
    $operation = if ($VerifyOnly) { 'Verify' } else { 'Flash' }
    throw "$operation failed with exit code $LASTEXITCODE."
}
