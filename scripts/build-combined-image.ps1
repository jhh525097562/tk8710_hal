param(
    [string]$CgtRoot
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

function Require-File {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing file: $Path"
    }
}

function Get-IHexSegments {
    param([string]$Path)

    $base = [uint64]0
    $segments = New-Object System.Collections.Generic.List[object]

    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line.Length -lt 11 -or $line[0] -ne ':') { continue }
        $len = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $type = [Convert]::ToInt32($line.Substring(7, 2), 16)

        if ($type -eq 2) {
            $base = [uint64]([Convert]::ToInt32($line.Substring(9, 4), 16)) * 16
            continue
        }
        if ($type -eq 4) {
            $base = [uint64]([Convert]::ToInt32($line.Substring(9, 4), 16)) * 65536
            continue
        }
        if ($type -eq 0 -and $len -gt 0) {
            $start = $base + [uint64]$addr
            $end = $start + [uint64]$len - 1
            $segments.Add([pscustomobject]@{ Min = $start; Max = $end })
        }
    }

    if ($segments.Count -eq 0) {
        throw "No data records found in $Path"
    }

    $sorted = $segments | Sort-Object Min, Max
    $merged = New-Object System.Collections.Generic.List[object]
    foreach ($seg in $sorted) {
        if ($merged.Count -eq 0) {
            $merged.Add([pscustomobject]@{ Min = $seg.Min; Max = $seg.Max })
            continue
        }

        $last = $merged[$merged.Count - 1]
        if ($seg.Min -le ($last.Max + 1)) {
            if ($seg.Max -gt $last.Max) { $last.Max = $seg.Max }
        } else {
            $merged.Add([pscustomobject]@{ Min = $seg.Min; Max = $seg.Max })
        }
    }

    return $merged
}

function Format-IHexSegments {
    param($Segments)

    (($Segments | ForEach-Object { '0x{0:X8}..0x{1:X8}' -f $_.Min, $_.Max }) -join ', ')
}

function Assert-NoOverlap {
    param(
        [string]$AName,
        $ASegments,
        [string]$BName,
        $BSegments
    )

    foreach ($a in $ASegments) {
        foreach ($b in $BSegments) {
            if ($a.Min -le $b.Max -and $b.Min -le $a.Max) {
                throw ('HEX ranges overlap: {0}=0x{1:X8}..0x{2:X8}, {3}=0x{4:X8}..0x{5:X8}' -f $AName, $a.Min, $a.Max, $BName, $b.Min, $b.Max)
            }
        }
    }
}

function Get-IHexDataBytes {
    param([string]$Path)

    $base = [uint64]0
    $bytes = @{}

    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line.Length -lt 11 -or $line[0] -ne ':') { continue }
        $len = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $type = [Convert]::ToInt32($line.Substring(7, 2), 16)

        if ($type -eq 2) {
            $base = [uint64]([Convert]::ToInt32($line.Substring(9, 4), 16)) * 16
            continue
        }
        if ($type -eq 4) {
            $base = [uint64]([Convert]::ToInt32($line.Substring(9, 4), 16)) * 65536
            continue
        }
        if ($type -eq 0 -and $len -gt 0) {
            for ($i = 0; $i -lt $len; $i++) {
                $absolute = $base + [uint64]$addr + [uint64]$i
                $bytes[$absolute.ToString()] = [Convert]::ToByte($line.Substring(9 + ($i * 2), 2), 16)
            }
        }
    }

    return $bytes
}

function New-IHexRecord {
    param(
        [int]$Address,
        [byte[]]$Data
    )

    $sum = $Data.Length + (($Address -shr 8) -band 0xFF) + ($Address -band 0xFF)
    $payload = ''
    foreach ($b in $Data) {
        $sum += $b
        $payload += '{0:X2}' -f $b
    }
    $checksum = ((-$sum) -band 0xFF)
    return ':{0:X2}{1:X4}00{2}{3:X2}' -f $Data.Length, $Address, $payload, $checksum
}

function New-IHexElaRecord {
    param([int]$Upper16)

    $b0 = ($Upper16 -shr 8) -band 0xFF
    $b1 = $Upper16 -band 0xFF
    $sum = 2 + 4 + $b0 + $b1
    $checksum = ((-$sum) -band 0xFF)
    return ':02000004{0:X4}{1:X2}' -f $Upper16, $checksum
}

function Write-FlashProgrammerHex {
    param(
        [string]$InputHex,
        [string]$OutputHex
    )

    $inputBytes = Get-IHexDataBytes $InputHex
    $outputBytes = @{}

    foreach ($key in $inputBytes.Keys) {
        $addr = [uint64]$key
        if ($addr -ge [uint64]4026531840) {
            continue
        }

        $outputBytes[$addr.ToString()] = $inputBytes[$key]
    }

    $records = New-Object System.Collections.Generic.List[string]
    $currentUpper = -1
    $addresses = $outputBytes.Keys | ForEach-Object { [uint64]$_ } | Sort-Object
    $i = 0

    while ($i -lt $addresses.Count) {
        $start = [uint64]$addresses[$i]
        $upper = [int](($start -shr 16) -band 0xFFFF)
        if ($upper -ne $currentUpper) {
            $records.Add((New-IHexElaRecord $upper))
            $currentUpper = $upper
        }

        $chunk = New-Object System.Collections.Generic.List[byte]
        $recordStart = [int]($start -band 0xFFFF)
        $addr = $start
        while (($i -lt $addresses.Count) -and ([uint64]$addresses[$i] -eq $addr) -and ($chunk.Count -lt 16) -and ((([uint64]$addresses[$i] -shr 16) -band 0xFFFF) -eq $currentUpper)) {
            $chunk.Add([byte]$outputBytes[([uint64]$addresses[$i]).ToString()])
            $addr++
            $i++
        }

        $records.Add((New-IHexRecord $recordStart $chunk.ToArray()))
    }

    $records.Add(':00000001FF')
    [System.IO.File]::WriteAllLines($OutputHex, $records, [System.Text.Encoding]::ASCII)
}

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

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$objcopy = if ($CgtRoot) { Join-Path $CgtRoot 'bin\armobjcopy.exe' } else { $null }
$bootOut = Join-Path $projectRoot 'bootloader\Debug\tms570ls3137_bootloader.out'
$appOut = Join-Path $projectRoot 'Debug\tms570ls3137_halcogen_base_570RAM.out'
$outDir = Join-Path $projectRoot 'combined'
$bootHex = Join-Path $outDir 'boot.hex'
$appHex = Join-Path $outDir 'app.hex'
$mergedHex = Join-Path $outDir 'tms570_boot_app.hex'
$flashHex = Join-Path $outDir 'tms570_boot_app_flash.hex'

Require-File $objcopy
Require-File $bootOut
Require-File $appOut
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

& $objcopy -O ihex $bootOut $bootHex
if ($LASTEXITCODE -ne 0) { throw 'armobjcopy failed for bootloader' }

& $objcopy -O ihex $appOut $appHex
if ($LASTEXITCODE -ne 0) { throw 'armobjcopy failed for app' }

$bootSegments = Get-IHexSegments $bootHex
$appSegments = Get-IHexSegments $appHex
Assert-NoOverlap 'boot' $bootSegments 'app' $appSegments

$merged = New-Object System.Collections.Generic.List[string]
foreach ($line in [System.IO.File]::ReadLines($bootHex)) {
    if ($line -ne ':00000001FF') { $merged.Add($line) }
}
foreach ($line in [System.IO.File]::ReadLines($appHex)) {
    if ($line -ne ':00000001FF') { $merged.Add($line) }
}
$merged.Add(':00000001FF')
[System.IO.File]::WriteAllLines($mergedHex, $merged, [System.Text.Encoding]::ASCII)

$mergedSegments = Get-IHexSegments $mergedHex
Write-FlashProgrammerHex $mergedHex $flashHex
$flashSegments = Get-IHexSegments $flashHex

Write-Host ('Boot HEX:   {0}  {1}' -f $bootHex, (Format-IHexSegments $bootSegments))
Write-Host ('App HEX:    {0}  {1}' -f $appHex, (Format-IHexSegments $appSegments))
Write-Host ('Merged HEX: {0}  {1}' -f $mergedHex, (Format-IHexSegments $mergedSegments))
Write-Host ('Flash HEX:  {0}  {1}' -f $flashHex, (Format-IHexSegments $flashSegments))
Write-Host 'Build combined image succeeded.'
