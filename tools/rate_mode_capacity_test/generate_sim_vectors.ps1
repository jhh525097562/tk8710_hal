[CmdletBinding()]
param(
    [string]$SourceDir = 'D:\data\K2301\K2301PHY_CaseList_Data\Simulation_Database\class4\case1',
    [string]$Output = (Join-Path $PSScriptRoot 'capacity_sim_vectors.h')
)

$ErrorActionPreference = 'Stop'

function Read-Numbers([string]$Name) {
    $path = Join-Path $SourceDir $Name
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing simulation vector: $path" }
    return [regex]::Matches([IO.File]::ReadAllText($path), '\d+') |
        ForEach-Object { [uint64]$_.Value }
}

function Format-Array([string]$Name, [byte[]]$Data) {
    $lines = for ($offset = 0; $offset -lt $Data.Length; $offset += 16) {
        $last = [Math]::Min($offset + 15, $Data.Length - 1)
        '    ' + (($Data[$offset..$last] | ForEach-Object { '0x{0:X2}U' -f $_ }) -join ', ')
    }
    "static const uint8_t ${Name}[$($Data.Length)] = {`r`n" +
        ($lines -join ",`r`n") + "`r`n};`r`n"
}

$anoiseValues = @(Read-Numbers 'ANoise.txt')
$ahValues = @(Read-Numbers 'GWRXAH.txt')
$pilotValues = @(Read-Numbers 'GWRxPilotPower.txt')
$freqValues = @(Read-Numbers 'TxFreq.txt')
$powerValues = @(Read-Numbers 'TxPower.txt')
if ($anoiseValues.Count -ne 8 -or $ahValues.Count -ne 2048 -or
    $pilotValues.Count -ne 128 -or $freqValues.Count -ne 128 -or
    $powerValues.Count -lt 128) {
    throw "Unexpected vector counts: anoise=$($anoiseValues.Count) ah=$($ahValues.Count) pilot=$($pilotValues.Count) freq=$($freqValues.Count) power=$($powerValues.Count)"
}

$anoise = New-Object byte[] 16
for ($i = 0; $i -lt 8; $i++) {
    $anoise[2*$i] = [byte](($anoiseValues[$i] -shr 8) -band 0xFF)
    $anoise[2*$i+1] = [byte]($anoiseValues[$i] -band 0xFF)
}

$ahList = [Collections.Generic.List[byte]]::new()
[uint64]$bitBuffer = 0; $bits = 0
foreach ($value in $ahValues) {
    $bitBuffer = (($bitBuffer -shl 20) -bor ($value -band 0xFFFFF))
    $bits += 20
    while ($bits -ge 8) {
        $ahList.Add([byte](($bitBuffer -shr ($bits - 8)) -band 0xFF))
        $bits -= 8
        if ($bits -eq 0) { $bitBuffer = 0 } else { $bitBuffer = $bitBuffer -band (([uint64]1 -shl $bits) - 1) }
    }
}

$pilot = New-Object byte[] 640
for ($i = 0; $i -lt 128; $i++) {
    for ($b = 0; $b -lt 5; $b++) { $pilot[5*$i+$b] = [byte](($pilotValues[$i] -shr (32-8*$b)) -band 0xFF) }
}
$freq = New-Object byte[] 512
for ($i = 0; $i -lt 128; $i++) {
    for ($b = 0; $b -lt 4; $b++) { $freq[4*$i+$b] = [byte](($freqValues[$i] -shr (24-8*$b)) -band 0xFF) }
}
$power = [byte[]]($powerValues[0..127] | ForEach-Object { [byte]$_ })

$sourceLabel = ($SourceDir -replace '\\','/')
$header = @"
#ifndef CAPACITY_SIM_VECTORS_H
#define CAPACITY_SIM_VECTORS_H
/* Generated from $sourceLabel using the packing
 * contract implemented by the existing DriverTest/Test8710Loopback.c. */
"@
$header += "`r`n" + (Format-Array 'g_capacityAnoise' $anoise)
$header += Format-Array 'g_capacityAh' ([byte[]]$ahList.ToArray())
$header += Format-Array 'g_capacityPilotPower' $pilot
$header += Format-Array 'g_capacityTxFreq' $freq
$header += Format-Array 'g_capacityTxPower' $power
$header += "#endif`r`n"
[IO.File]::WriteAllText($Output, $header, [Text.UTF8Encoding]::new($false))
Write-Host "Generated $Output (AH=$($ahList.Count) bytes)"
