param(
    [switch]$Clean,

    [string]$CgtRoot,

    [string]$F021Root
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
$bootRoot = Join-Path $projectRoot 'bootloader'
$buildDir = Join-Path $bootRoot 'Debug'

if ([string]::IsNullOrWhiteSpace($CgtRoot)) {
    $CgtRoot = Find-FirstPath @(
        'F:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'D:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'C:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS',
        'F:\ti\ti-cgt-arm_20.2.7.LTS',
        'D:\ti\ti-cgt-arm_20.2.7.LTS',
        'C:\ti\ti-cgt-arm_20.2.7.LTS'
    )
}

if ([string]::IsNullOrWhiteSpace($F021Root)) {
    $F021Root = Find-FirstPath @(
        'F:\ti\Hercules\F021 Flash API\02.01.01',
        'D:\ti\Hercules\F021 Flash API\02.01.01',
        'C:\ti\Hercules\F021 Flash API\02.01.01',
        'C:\ti\Hercules\F021 Flash API\02.01.01'
    )
}

$armcl = if ($CgtRoot) { Join-Path $CgtRoot 'bin\armcl.exe' } else { $null }
if (([string]::IsNullOrWhiteSpace($armcl)) -or (-not (Test-Path -LiteralPath $armcl))) {
    throw 'TI ARM CGT armcl.exe was not found. Pass -CgtRoot <path-to-ti-cgt-arm_20.2.7.LTS>.'
}

$f021Lib = if ($F021Root) { Join-Path $F021Root 'F021_API_CortexR4_BE_V3D16.lib' } else { $null }
$f021Include = if ($F021Root) { Join-Path $F021Root 'include' } else { $null }
if (([string]::IsNullOrWhiteSpace($f021Lib)) -or (-not (Test-Path -LiteralPath $f021Lib))) {
    throw 'F021_API_CortexR4_BE_V3D16.lib was not found. Install Hercules F021 Flash API 02.01.01 or pass -F021Root <path>.'
}
if (-not (Test-Path -LiteralPath $f021Include)) {
    throw "F021 include directory was not found: $f021Include"
}

if ($Clean) {
    if (Test-Path -LiteralPath $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    Write-Host "Cleaned bootloader build directory: $buildDir"
    return
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Write-Host "Project: $projectRoot"
Write-Host "Bootloader: $bootRoot"
Write-Host "TI CGT: $CgtRoot"
Write-Host "F021 API: $F021Root"
Write-Host "armcl: $armcl"

& $armcl --compiler_revision
if ($LASTEXITCODE -ne 0) {
    throw "armcl revision check failed with exit code $LASTEXITCODE."
}

$sourceFiles = @(
    'source/bl_main.c',
    'source/bl_led_demo.c',
    'source/bl_flash.c',
    'source/bl_dcan.c',
    'source/bl_check.c',
    'source/sci_common.c',
    'source/Fapi_UserDefinedFunctions.c',
    'source/bl_startup_state.c',
    'source/can.c',
    'source/dabort.asm',
    'source/emif.c',
    'source/errata_SSWF021_45.c',
    'source/esm.c',
    'source/gio.c',
    'source/het.c',
    'source/notification.c',
    'source/pinmux.c',
    'source/sci.c',
    'source/sys_core.asm',
    'source/sys_intvecs.asm',
    'source/sys_mpu.asm',
    'source/sys_phantom.c',
    'source/sys_pmu.asm',
    'source/sys_selftest.c',
    'source/sys_startup.c',
    'source/sys_vim.c',
    'source/system.c'
)

$includePaths = @(
    $bootRoot,
    (Join-Path $bootRoot 'include'),
    (Join-Path $bootRoot 'TMS570LS31x'),
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
    '-g',
    '--c99',
    '--diag_warning=225',
    '--diag_wrap=off',
    '--display_error_number',
    '--enum_type=packed',
    '--abi=eabi'
)

$objects = @()
foreach ($relativeSource in $sourceFiles) {
    $sourcePath = Join-Path $bootRoot $relativeSource
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Bootloader source file not found: $sourcePath"
    }

    $relativeDir = Split-Path $relativeSource -Parent
    $objectDir = Join-Path $buildDir $relativeDir
    New-Item -ItemType Directory -Path $objectDir -Force | Out-Null

    Write-Host "Building bootloader file: $relativeSource"
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

$outFile = Join-Path $buildDir 'tms570ls3137_bootloader.out'
$mapFile = Join-Path $buildDir 'tms570ls3137_bootloader.map'
$xmlFile = Join-Path $buildDir 'tms570ls3137_bootloader_linkInfo.xml'
$linkerTemplate = Join-Path $bootRoot 'TMS570LS31x\bl_link.cmd'
$linkerCmd = Join-Path $buildDir 'bl_link_resolved.cmd'
$normalizedF021Lib = $f021Lib.Replace('\', '/')
$linkerContent = Get-Content -LiteralPath $linkerTemplate -Raw
if ($linkerContent -notmatch '__F021_LIBRARY__') {
    throw "Bootloader linker template is missing __F021_LIBRARY__: $linkerTemplate"
}
$linkerContent = $linkerContent.Replace('__F021_LIBRARY__', $normalizedF021Lib)
Set-Content -LiteralPath $linkerCmd -Value $linkerContent -NoNewline

Write-Host "Building bootloader target: $outFile"
$linkArgs = $commonArgs + @(
    '-z',
    "-m$mapFile",
    '--heap_size=0x1000',
    '--stack_size=0x1000',
    "-i$(Join-Path $CgtRoot 'lib')",
    "-i$(Join-Path $CgtRoot 'include')",
    "-i$F021Root",
    '--reread_libs',
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
    throw "Bootloader link failed with exit code $LASTEXITCODE."
}

Write-Host "Finished building bootloader target: $outFile"
