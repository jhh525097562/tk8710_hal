[CmdletBinding()]
param(
    [ValidateRange(1, 100000)]
    [uint32]$Frames = 10,

    [ValidateRange(100, 600000)]
    [uint32]$TimeoutMs = 5000,

    [ValidateSet('All', '0', '1', '2')]
    [string]$Rate = 'All',

    [ValidateSet('All', '16', '128')]
    [string]$Users = 'All',

    [string]$CcsRoot,
    [string]$CgtRoot,
    [string]$F021Root,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

function Find-FirstPath {
    param([string[]]$Paths)
    foreach ($candidate in $Paths) {
        if ((-not [string]::IsNullOrWhiteSpace($candidate)) -and
            (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

$toolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $toolRoot '..\..')).Path
$outputDir = Join-Path $toolRoot 'build-tms570'

if ($Clean) {
    if (Test-Path -LiteralPath $outputDir) {
        Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
    Write-Host "Cleaned $outputDir"
    return
}

if ([string]::IsNullOrWhiteSpace($CcsRoot)) {
    $CcsRoot = Find-FirstPath @(
        'D:\ti\ccs1281\ccs', 'C:\ti\ccs1281\ccs',
        'D:\ti\ccs1260\ccs', 'C:\ti\ccs1260\ccs'
    )
}
if ([string]::IsNullOrWhiteSpace($CgtRoot)) {
    $CgtRoot = Find-FirstPath @(
        $(if ($CcsRoot) { Join-Path $CcsRoot 'tools\compiler\ti-cgt-arm_20.2.7.LTS' }),
        'D:\ti\ti-cgt-arm_20.2.7.LTS', 'C:\ti\ti-cgt-arm_20.2.7.LTS'
    )
}
if ([string]::IsNullOrWhiteSpace($F021Root)) {
    $F021Root = Find-FirstPath @(
        'D:\ti\Hercules\F021 Flash API\02.01.01',
        'C:\ti\Hercules\F021 Flash API\02.01.01'
    )
}

$armcl = if ($CgtRoot) { Join-Path $CgtRoot 'bin\armcl.exe' } else { $null }
$f021Lib = if ($F021Root) { Join-Path $F021Root 'F021_API_CortexR4_BE_V3D16.lib' } else { $null }
$f021Include = if ($F021Root) { Join-Path $F021Root 'include' } else { $null }
if ((-not $armcl) -or (-not (Test-Path -LiteralPath $armcl))) {
    throw 'TI ARM Compiler 20.2.7.LTS was not found. Pass -CgtRoot.'
}
if ((-not $f021Lib) -or (-not (Test-Path -LiteralPath $f021Lib)) -or
    (-not (Test-Path -LiteralPath $f021Include))) {
    throw 'Hercules F021 Flash API 02.01.01 was not found. Pass -F021Root.'
}

# Build the existing sources first, without editing the managed project files.
$baseBuild = Join-Path $projectRoot 'scripts\build-tms570.ps1'
$baseArgs = @{
    Configuration = 'Debug'
    DataTransferSpi2SlaveTest = $true
    CgtRoot = $CgtRoot
    F021Root = $F021Root
}
if ($CcsRoot) {
    $baseArgs.CcsRoot = $CcsRoot
}
& $baseBuild @baseArgs
if ($LASTEXITCODE -ne 0) {
    throw "Base TMS570 object build failed with exit code $LASTEXITCODE."
}

$baseObjectRoot = Join-Path $projectRoot 'Debug'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$includePaths = @(
    $projectRoot,
    (Join-Path $projectRoot 'include'),
    (Join-Path $projectRoot 'rk3506_8710\inc'),
    (Join-Path $projectRoot 'rk3506_8710\inc\driver'),
    (Join-Path $projectRoot 'rk3506_8710\inc\phy'),
    (Join-Path $projectRoot 'rk3506_8710\inc\trm'),
    (Join-Path $projectRoot 'rk3506_8710\port'),
    $toolRoot,
    $f021Include,
    (Join-Path $CgtRoot 'include')
)

$commonArgs = @('-mv7R4', '--code_state=32', '--float_support=VFPv3D16', '-O2')
foreach ($path in $includePaths) {
    $commonArgs += "--include_path=$path"
}
$commonArgs += @(
    '--define=PLATFORM_TMS570',
    '--define=TK8710_TMS570_PAYLOAD_ONLY',
    '--define=TK8710_DRIVER_TEST_RX_INJECTION',
    '--define=DATA_TRANSFER_SPI2_SLAVE_TEST',
    "--define=CAPACITY_TEST_FRAMES=$Frames",
    "--define=CAPACITY_TEST_TIMEOUT_MS=$TimeoutMs",
    '-g', '--c99', '--diag_warning=225', '--diag_wrap=off',
    '--display_error_number', '--enum_type=packed', '--abi=eabi'
)
if ($Rate -ne 'All') {
    $commonArgs += "--define=CAPACITY_TEST_PROTOCOL_RATE=$Rate"
}
if ($Users -ne 'All') {
    $commonArgs += "--define=CAPACITY_TEST_USER_COUNT=$Users"
}

$toolObjects = @()
foreach ($sourceName in @('capacity_test_logic.c', 'rate_mode_capacity_test.c')) {
    $source = Join-Path $toolRoot $sourceName
    & $armcl @commonArgs --preproc_with_compile "--obj_directory=$outputDir" $source
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed for $sourceName with exit code $LASTEXITCODE."
    }
    $toolObjects += Join-Path $outputDir (([IO.Path]::GetFileNameWithoutExtension($sourceName)) + '.obj')
}

# Recompile only the IRQ implementation with the private test-injection macro.
# The production object is excluded below, so production builds remain unchanged.
$testIrqSource = Join-Path $projectRoot 'rk3506_8710\src\driver\tk8710_irq.c'
$testIrqObject = Join-Path $outputDir 'tk8710_irq_test.obj'
& $armcl @commonArgs --preproc_with_compile "--output_file=$testIrqObject" $testIrqSource
if ($LASTEXITCODE -ne 0) {
    throw "Compile failed for test-only tk8710_irq.c with exit code $LASTEXITCODE."
}
$toolObjects += $testIrqObject

# Production entry/application objects are deliberately excluded. Driver logging
# still needs data_transfer.obj and its SPI flash dependency, so those remain.
$excludedRelativeObjects = @(
    'source\sys_main.obj',
    'source\external_watchdog.obj',
    'source\fpga_param_store.obj',
    'source\fpga_protocol.obj',
    'source\tk8710_sat_payload_app.obj'
    'rk3506_8710\src\driver\tk8710_irq.obj'
)
$baseObjects = Get-ChildItem -LiteralPath $baseObjectRoot -Recurse -File -Filter '*.obj' |
    Where-Object {
        $relative = $_.FullName.Substring($baseObjectRoot.Length + 1)
        $excludedRelativeObjects -notcontains $relative
    } |
    Select-Object -ExpandProperty FullName
if ($baseObjects.Count -eq 0) {
    throw "No base objects were found under $baseObjectRoot."
}

$outFile = Join-Path $outputDir 'rate_mode_capacity_test.out'
$mapFile = Join-Path $outputDir 'rate_mode_capacity_test.map'
$xmlFile = Join-Path $outputDir 'rate_mode_capacity_test_linkInfo.xml'
$linkerCmd = Join-Path $projectRoot 'source\sys_link.cmd'
$linkArgs = $commonArgs + @(
    '-z', "-m$mapFile", '--heap_size=0x1000', '--stack_size=0x1000',
    "-i$(Join-Path $CgtRoot 'lib')", "-i$(Join-Path $CgtRoot 'include')",
    "-i$F021Root", '--reread_libs', '--define=source/sys_link.cmd',
    '--diag_wrap=off', '--display_error_number', '--warn_sections',
    "--xml_link_info=$xmlFile", '--rom_model', '--be32', '-o', $outFile
) + $baseObjects + $toolObjects + @(
    $linkerCmd, '-lF021_API_CortexR4_BE_V3D16.lib',
    '-lrtsv7R4_T_be_v3D16_eabi.lib'
)

& $armcl @linkArgs
if ($LASTEXITCODE -ne 0) {
    throw "Link failed with exit code $LASTEXITCODE."
}

Write-Host "Built standalone capacity-test firmware: $outFile"
Write-Host "Frames=$Frames TimeoutMs=$TimeoutMs Rate=$Rate Users=$Users"
