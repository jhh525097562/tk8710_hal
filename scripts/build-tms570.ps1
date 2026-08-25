[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [switch]$Clean,

    [string]$CcsRoot,

    [string]$CgtRoot,

    [string]$F021Root,

    [switch]$FpgaSelfTest,

    [switch]$DataTransferSpi2SlaveTest
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

function Find-InPath {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    return $null
}

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $projectRoot $Configuration
$makefile = Join-Path $buildDir 'makefile'
$directBuildMarker = Join-Path $buildDir '.direct-build-test-flags'

if ([string]::IsNullOrWhiteSpace($CcsRoot)) {
    $CcsRoot = Find-FirstPath @(
        'F:\ti\ccs1281\ccs',
        'D:\ti\ccs1281\ccs',
        'C:\ti\ccs1281\ccs',
        'D:\ti\ccs1260\ccs',
        'C:\ti\ccs1260\ccs',
        'D:\ti\ccs\ccs',
        'C:\ti\ccs\ccs'
    )
}

if ([string]::IsNullOrWhiteSpace($CgtRoot)) {
    $cgtCandidates = @(
        'F:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'D:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'C:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'F:\ti\ti-cgt-arm_20.2.7.LTS',
        'D:\ti\ti-cgt-arm_20.2.7.LTS',
        'C:\ti\ti-cgt-arm_20.2.7.LTS'
    )
    if (-not [string]::IsNullOrWhiteSpace($CcsRoot)) {
        $cgtCandidates = @((Join-Path $CcsRoot 'tools\compiler\ti-cgt-arm_20.2.7.LTS')) + $cgtCandidates
    }
    $CgtRoot = Find-FirstPath $cgtCandidates
}

$armcl = if ($CgtRoot) { Join-Path $CgtRoot 'bin\armcl.exe' } else { Find-InPath 'armcl.exe' }
if (([string]::IsNullOrWhiteSpace($armcl)) -or (-not (Test-Path -LiteralPath $armcl))) {
    throw "TI ARM CGT armcl.exe was not found. Install TI ARM Compiler 20.2.7.LTS with CCS, or pass -CgtRoot <path-to-ti-cgt-arm_20.2.7.LTS>."
}

if ([string]::IsNullOrWhiteSpace($F021Root)) {
    $F021Root = Find-FirstPath @(
        'F:\ti\Hercules\F021 Flash API\02.01.01',
        'D:\ti\Hercules\F021 Flash API\02.01.01',
        'C:\ti\Hercules\F021 Flash API\02.01.01'
    )
}
$f021Lib = if ($F021Root) { Join-Path $F021Root 'F021_API_CortexR4_BE_V3D16.lib' } else { $null }
$f021Include = if ($F021Root) { Join-Path $F021Root 'include' } else { $null }
if (([string]::IsNullOrWhiteSpace($f021Lib)) -or (-not (Test-Path -LiteralPath $f021Lib))) {
    throw 'F021_API_CortexR4_BE_V3D16.lib was not found. Install Hercules F021 Flash API 02.01.01 or pass -F021Root <path>.'
}
if (([string]::IsNullOrWhiteSpace($f021Include)) -or (-not (Test-Path -LiteralPath $f021Include))) {
    throw "F021 include directory was not found: $f021Include"
}

$gmake = $null
if ($CcsRoot) {
    $candidate = Join-Path $CcsRoot 'utils\bin\gmake.exe'
    if (Test-Path -LiteralPath $candidate) {
        $gmake = $candidate
    }
}
if (-not $gmake) {
    $gmake = Find-InPath 'gmake.exe'
}
if (-not $gmake) {
    $gmake = Find-InPath 'make.exe'
}
if (([string]::IsNullOrWhiteSpace($gmake)) -or (-not (Test-Path -LiteralPath $gmake))) {
    throw "GNU make was not found. Install Code Composer Studio for gmake.exe, or install make.exe and ensure it is on PATH."
}

$env:PATH = (Join-Path $CgtRoot 'bin') + ';' + (Split-Path -Parent $gmake) + ';' + $env:PATH

$normalizedCgtRoot = $CgtRoot.Replace('\', '/')
$normalizedProjectRoot = $projectRoot.Replace('\', '/')
$generatedMakefiles = @()
if (Test-Path -LiteralPath $buildDir) {
    $generatedMakefiles = Get-ChildItem -LiteralPath $buildDir -Recurse -File -Include '*.mk', 'makefile'
}

foreach ($file in $generatedMakefiles) {
    $content = Get-Content -LiteralPath $file.FullName -Raw
    $updated = $content `
        -replace 'D:/ti/ccs1281/ccs/tools/compiler/ti-cgt-arm_20\.2\.7\.LTS', $normalizedCgtRoot `
        -replace 'C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-arm_20\.2\.7\.LTS', $normalizedCgtRoot `
        -replace 'D:/ti_workspace/tms570ls3137_halcogen_base_570RAM(?:_add)*', $normalizedProjectRoot `
        -replace 'C:/ti_workspace/tms570ls3137_halcogen_base_570RAM(?:_add)*', $normalizedProjectRoot
    if ($updated -ne $content) {
        Set-Content -LiteralPath $file.FullName -Value $updated -NoNewline
    }
}

Write-Host "Project: $projectRoot"
Write-Host "Configuration: $Configuration"
Write-Host "TI CGT: $CgtRoot"
Write-Host "F021 API: $F021Root"
Write-Host "armcl: $armcl"
Write-Host "make: $gmake"

& $armcl --compiler_revision
if ($LASTEXITCODE -ne 0) {
    throw "armcl revision check failed with exit code $LASTEXITCODE."
}

if ((Test-Path -LiteralPath $makefile) -and (-not $FpgaSelfTest) -and (-not $DataTransferSpi2SlaveTest)) {
    if ((-not $Clean) -and (Test-Path -LiteralPath $directBuildMarker)) {
        Write-Host 'Previous output used direct-build test flags; cleaning managed objects before the normal build.'
        & $gmake -C $buildDir clean
        if ($LASTEXITCODE -ne 0) {
            throw "Managed clean failed with exit code $LASTEXITCODE."
        }
        Remove-Item -LiteralPath $directBuildMarker -Force
    }
    $target = if ($Clean) { 'clean' } else { 'all' }
    & $gmake -C $buildDir $target
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE. If errors mention D:/ti_workspace or a missing compiler path, regenerate the managed makefiles in CCS after importing this project into the current workspace."
    }
    return
}

if ($FpgaSelfTest -or $DataTransferSpi2SlaveTest) {
    Write-Host 'Test build flags requested; using full direct armcl build so the defines are guaranteed to apply.'
}
if ($FpgaSelfTest) {
    Write-Warning 'FPGA_PROTOCOL_SELF_TEST disables TK8710 SPI reset/version bring-up and TK8710 IRQ/runtime processing. Do not use it when testing the real TK8710.'
}

if ($Clean) {
    if (Test-Path -LiteralPath $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    Write-Host "Cleaned direct build directory: $buildDir"
    return
}

Write-Host "Generated CCS makefile not found. Using direct armcl build fallback."

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$projectName = Split-Path -Leaf $projectRoot
$projectFile = Join-Path $projectRoot '.project'
if (Test-Path -LiteralPath $projectFile) {
    try {
        [xml]$projectXml = Get-Content -LiteralPath $projectFile -Raw
        if (-not [string]::IsNullOrWhiteSpace($projectXml.projectDescription.name)) {
            $projectName = $projectXml.projectDescription.name
        }
    } catch {
        Write-Host "Warning: failed to read .project name, using directory name."
    }
}

$sourceFiles = @(
    'rk3506_8710/port/tk8710_tms570.c',
    'rk3506_8710/src/driver/tk8710_config.c',
    'rk3506_8710/src/driver/tk8710_core.c',
    'rk3506_8710/src/driver/tk8710_irq.c',
    'rk3506_8710/src/driver/tk8710_log.c',
    'rk3506_8710/src/driver/tk8710_reg_pack.c',
    'rk3506_8710/src/hal/hal_api.c',
    'rk3506_8710/src/hal/hal_cb.c',
    'rk3506_8710/src/hal/hal_status.c',
    'rk3506_8710/src/phy/phy_api.c',
    'rk3506_8710/src/phy/phy_irq.c',
    'rk3506_8710/src/phy/phy_log.c',
    'rk3506_8710/src/phy/phy_regs.c',
    'rk3506_8710/src/phy/phy_test.c',
    'rk3506_8710/src/trm/phy_cfg.c',
    'rk3506_8710/src/trm/phy_data.c',
    'rk3506_8710/src/trm/phy_stat.c',
    'rk3506_8710/src/trm/trm_beam.c',
    'rk3506_8710/src/trm/trm_core.c',
    'rk3506_8710/src/trm/trm_data.c',
    'rk3506_8710/src/trm/trm_freq.c',
    'rk3506_8710/src/trm/trm_log.c',
    'rk3506_8710/src/trm/trm_mac_parser.c',
    'rk3506_8710/src/trm/trm_power.c',
    'rk3506_8710/src/trm/trm_queue.c',
    'rk3506_8710/src/trm/trm_satellite.c',
    'rk3506_8710/src/trm/trm_slot.c',
    'source/adc.c',
    'source/app_status.c',
    'source/can.c',
    'source/crc.c',
    'source/dabort.asm',
    'source/dcc.c',
    'source/dmm.c',
    'source/emac.c',
    'source/emif.c',
    'source/errata_SSWF021_45.c',
    'source/esm.c',
    'source/data_transfer.c',
    'source/fpga_param_store.c',
    'source/fpga_protocol.c',
    'source/gio.c',
    'source/het.c',
    'source/i2c.c',
    'source/lin.c',
    'source/mdio.c',
    'source/mibspi.c',
    'source/notification.c',
    'source/phy_dp83640.c',
    'source/pinmux.c',
    'source/pom.c',
    'source/rti.c',
    'source/rtp.c',
    'source/sci.c',
    'source/spi.c',
    'source/sys_core.asm',
    'source/sys_dma.c',
    'source/sys_intvecs.asm',
    'source/sys_main.c',
    'source/sys_mpu.asm',
    'source/sys_pcr.c',
    'source/sys_phantom.c',
    'source/sys_pmm.c',
    'source/sys_pmu.asm',
    'source/sys_selftest.c',
    'source/sys_startup.c',
    'source/sys_vim.c',
    'source/system.c',
    'source/spi_flash.c',
    'source/tk8710_sat_payload_app.c'
)

$includePaths = @(
    $projectRoot,
    (Join-Path $projectRoot 'include'),
    (Join-Path $projectRoot 'rk3506_8710/inc'),
    (Join-Path $projectRoot 'rk3506_8710/inc/driver'),
    (Join-Path $projectRoot 'rk3506_8710/inc/phy'),
    (Join-Path $projectRoot 'rk3506_8710/inc/trm'),
    (Join-Path $projectRoot 'rk3506_8710/port'),
    $f021Include,
    (Join-Path $CgtRoot 'include')
)

$commonArgs = @(
    '-mv7R4',
    '--code_state=32',
    '--float_support=VFPv3D16',
    '-O2'
)
foreach ($includePath in $includePaths) {
    $commonArgs += "--include_path=$includePath"
}
$commonArgs += @(
    '--define=PLATFORM_TMS570',
    '--define=TK8710_TMS570_PAYLOAD_ONLY',
    '-g',
    '--c99',
    '--diag_warning=225',
    '--diag_wrap=off',
    '--display_error_number',
    '--enum_type=packed',
    '--abi=eabi'
)
if ($FpgaSelfTest) {
    $commonArgs += '--define=FPGA_PROTOCOL_SELF_TEST'
}
if ($DataTransferSpi2SlaveTest) {
    $commonArgs += '--define=DATA_TRANSFER_SPI2_SLAVE_TEST'
}

$enabledTestFlags = @()
if ($FpgaSelfTest) { $enabledTestFlags += 'FPGA_PROTOCOL_SELF_TEST' }
if ($DataTransferSpi2SlaveTest) { $enabledTestFlags += 'DATA_TRANSFER_SPI2_SLAVE_TEST' }
if ($enabledTestFlags.Count -gt 0) {
    Set-Content -LiteralPath $directBuildMarker -Value ($enabledTestFlags -join "`r`n")
}
$objects = @()
foreach ($relativeSource in $sourceFiles) {
    $sourcePath = Join-Path $projectRoot $relativeSource
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Source file not found: $sourcePath"
    }

    $relativeDir = Split-Path $relativeSource -Parent
    $objectDir = Join-Path $buildDir $relativeDir
    New-Item -ItemType Directory -Path $objectDir -Force | Out-Null

    Write-Host "Building file: $relativeSource"
    $compileArgs = $commonArgs + @(
        '--preproc_with_compile',
        "--obj_directory=$objectDir",
        $sourcePath
    )
    & $armcl @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed for $relativeSource with exit code $LASTEXITCODE."
    }

    $objectName = [System.IO.Path]::GetFileNameWithoutExtension($relativeSource) + '.obj'
    $objects += (Join-Path $objectDir $objectName)
}

$outFile = Join-Path $buildDir ($projectName + '.out')
$mapFile = Join-Path $buildDir ($projectName + '.map')
$xmlFile = Join-Path $buildDir ($projectName + '_linkInfo.xml')
$linkerCmd = Join-Path $projectRoot 'source/sys_link.cmd'

Write-Host "Building target: $outFile"
$linkArgs = $commonArgs + @(
    '-z',
    "-m$mapFile",
    '--heap_size=0x1000',
    '--stack_size=0x1000',
    "-i$(Join-Path $CgtRoot 'lib')",
    "-i$(Join-Path $CgtRoot 'include')",
    "-i$F021Root",
    '--reread_libs',
    '--define=source/sys_link.cmd',
    '--diag_wrap=off',
    '--display_error_number',
    '--warn_sections',
    "--xml_link_info=$xmlFile",
    '--rom_model',
    '--be32',
    '-o',
    $outFile
) + $objects + @($linkerCmd, '-lF021_API_CortexR4_BE_V3D16.lib', '-lrtsv7R4_T_be_v3D16_eabi.lib')

& $armcl @linkArgs
if ($LASTEXITCODE -ne 0) {
    throw "Link failed with exit code $LASTEXITCODE."
}

Write-Host "Finished building target: $outFile"
