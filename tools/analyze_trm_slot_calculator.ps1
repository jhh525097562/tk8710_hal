param(
    [string]$OutputDirectory = "docs/trm_slot_analysis"
)

$ErrorActionPreference = "Stop"

$modes = @(5, 6, 7, 8, 9, 10, 11, 18)
$bcnLen = @{ 5=69583; 6=36340; 7=19129; 8=10732; 9=6510; 10=6510; 11=6510; 18=6510 }
$body = @{ 5=131072; 6=65536; 7=32768; 8=16384; 9=8192; 10=4096; 11=2048; 18=2048 }
$brdGap = @{ 5=21492; 6=19728; 7=12000; 8=5600; 9=2800; 10=1400; 11=800; 18=800 }
$ulGap = $brdGap
$dlGap = @{ 5=65000; 6=31500; 7=14500; 8=8000; 9=4500; 10=1400; 11=800; 18=800 }
$rateKHz = @{ 5=2; 6=4; 7=8; 8=16; 9=32; 10=64; 11=128; 18=128 }
$bandwidthKHz = @{ 5=62.5; 6=125; 7=250; 8=500; 9=500; 10=500; 11=500; 18=500 }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

function Get-SlotResult([int]$mode, [int]$superFrameNum, [int]$ulBlocks, [int]$dlBlocks) {
    # Match the active ground-WAN gateway path: calcType=GROUND_WAN, brdBlockNum=2.
    $brdLen = 1024L + $body[$mode] * 5L + $brdGap[$mode]
    $ulLen = 1024L + $body[$mode] * (2L * $ulBlocks + 1L) + $ulGap[$mode]
    $dlLen = 1024L + $body[$mode] * (2L * $dlBlocks + 1L) + $dlGap[$mode]
    $raw = $bcnLen[$mode] + $brdLen + $ulLen + $dlLen
    $bestPeriod = 0L
    $bestCount = 0
    $minimumGap = [long]::MaxValue
    for ($seconds = 1; $seconds -le 10; $seconds++) {
        $total = $seconds * 1000000L
        for ($count = 1; $count -le 64; $count++) {
            if (($total % $count) -ne 0) { continue }
            $period = [long]($total / $count)
            if ($period -lt $raw) { continue }
            $gap = $period - $raw
            if (($gap -lt $minimumGap) -and (($count % $superFrameNum) -eq 0)) {
                $minimumGap = $gap
                $bestPeriod = $period
                $bestCount = $count
            }
        }
    }
    $fallback = $bestPeriod -eq 0
    if ($fallback) {
        $bestPeriod = $raw
        $bestCount = 1
        $minimumGap = 0
    }
    [pscustomobject]@{
        rate_mode = $mode
        physical_rate_khz = $rateKHz[$mode]
        bandwidth_khz = $bandwidthKHz[$mode]
        super_frame_num = $superFrameNum
        brd_blocks = 2
        ul_blocks = $ulBlocks
        dl_blocks = $dlBlocks
        total_ul_dl_blocks = $ulBlocks + $dlBlocks
        raw_period_us = $raw
        frame_period_us = $bestPeriod
        frame_count = $bestCount
        added_gap_us = $minimumGap
        normal_solution = if ($fallback) { 0 } else { 1 }
        total_duration_us = $bestPeriod * $bestCount
    }
}

$detailPath = Join-Path $OutputDirectory "ground_wan_brd2_full_results.csv"
$summaryPath = Join-Path $OutputDirectory "ground_wan_brd2_support_summary.csv"

$allResults = foreach ($mode in $modes) {
    foreach ($superFrameNum in 1..100) {
        foreach ($ulBlocks in 1..16) {
            foreach ($dlBlocks in 1..16) {
                Get-SlotResult $mode $superFrameNum $ulBlocks $dlBlocks
            }
        }
    }
}
$detail = @($allResults | Where-Object normal_solution -eq 1)
$detail | Export-Csv -LiteralPath $detailPath -NoTypeInformation -Encoding UTF8

$summary = $allResults | Group-Object rate_mode, super_frame_num | ForEach-Object {
    $rows = $_.Group
    $normal = @($rows | Where-Object normal_solution -eq 1)
    [pscustomobject]@{
        rate_mode = $rows[0].rate_mode
        physical_rate_khz = $rows[0].physical_rate_khz
        bandwidth_khz = $rows[0].bandwidth_khz
        super_frame_num = $rows[0].super_frame_num
        tested_combinations = $rows.Count
        normal_combinations = $normal.Count
        fallback_combinations = $rows.Count - $normal.Count
        max_supported_ul_dl_sum = if ($normal.Count) { ($normal | Measure-Object total_ul_dl_blocks -Maximum).Maximum } else { 0 }
    }
}
$summary | Sort-Object rate_mode, super_frame_num | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8

Write-Output "detail=$detailPath rows=$($detail.Count)"
Write-Output "summary=$summaryPath rows=$($summary.Count)"
