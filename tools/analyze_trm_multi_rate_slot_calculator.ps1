param(
    [string]$OutputDirectory = "docs/trm_slot_analysis"
)

$ErrorActionPreference = "Stop"

$supportedDataModes = [ordered]@{
    '5' = @(6, 7, 8, 9)
    '6' = @(7, 8, 9, 10)
    '7' = @(8, 9, 10, 11, 18)
    '8' = @(9, 10, 11, 18)
    '9' = @(10, 11, 18)
}
$bcnLen = @{ 5=69583L; 6=36340L; 7=19129L; 8=10732L; 9=6510L; 10=6510L; 11=6510L; 18=6510L }
$body = @{ 5=131072L; 6=65536L; 7=32768L; 8=16384L; 9=8192L; 10=4096L; 11=2048L; 18=2048L }
$dataGap = @{ 5=21492L; 6=19728L; 7=12000L; 8=5600L; 9=2800L; 10=1400L; 11=800L; 18=800L }
$dlGap = @{ 5=65000L; 6=31500L; 7=14500L; 8=8000L; 9=4500L; 10=1400L; 11=800L; 18=800L }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

function Get-Combinations([int[]]$Values, [int]$Choose, [int]$Start = 0, [int[]]$Prefix = @()) {
    if ($Choose -eq 0) {
        [pscustomobject]@{ Values = [int[]]$Prefix }
        return
    }
    for ($index = $Start; $index -le $Values.Count - $Choose; $index++) {
        Get-Combinations $Values ($Choose - 1) ($index + 1) ($Prefix + $Values[$index])
    }
}

function Get-RateRawPeriod([int]$Mode, [int]$UlDlSum) {
    # Ground WAN, two broadcast blocks for every configured rate.
    $brd = 1024L + 5L * $body[$Mode] + $dataGap[$Mode]
    $ulDlFixed = 2048L + 2L * $body[$Mode] + $dataGap[$Mode] + $dlGap[$Mode]
    return $bcnLen[$Mode] + $brd + $ulDlFixed + 2L * $body[$Mode] * $UlDlSum
}

function Get-Candidates([int]$SuperFrameNum) {
    $periods = [System.Collections.Generic.List[long]]::new()
    for ($seconds = 1; $seconds -le 10; $seconds++) {
        $total = 1000000L * $seconds
        for ($count = 1; $count -le 64; $count++) {
            if (($count % $SuperFrameNum) -ne 0 -or ($total % $count) -ne 0) { continue }
            $periods.Add([long]($total / $count))
        }
    }
    return @($periods | Sort-Object -Unique)
}

$rateCombinations = foreach ($rateCount in 2..4) {
    foreach ($baseMode in $supportedDataModes.Keys) {
        foreach ($tail in @(Get-Combinations $supportedDataModes[[string]$baseMode] ($rateCount - 1))) {
            $modes = @([int]$baseMode) + @($tail.Values)
            [pscustomobject]@{
                rate_count = $rateCount
                base_bcn_mode = [int]$baseMode
                rate_modes = ($modes -join '+')
                mode_1 = $modes[0]
                mode_2 = $modes[1]
                mode_3 = if ($rateCount -ge 3) { $modes[2] } else { '' }
                mode_4 = if ($rateCount -ge 4) { $modes[3] } else { '' }
            }
        }
    }
}

$combinationPath = Join-Path $OutputDirectory "multi_rate_allowed_combinations.csv"
$summaryPath = Join-Path $OutputDirectory "multi_rate_block_support_summary_sf1.csv"
$supportPath = Join-Path $OutputDirectory "multi_rate_block_support_sf1.csv"
$rateCombinations | Export-Csv -LiteralPath $combinationPath -NoTypeInformation -Encoding UTF8

$supportRows = foreach ($combo in $rateCombinations) {
    $modes = @($combo.mode_1, $combo.mode_2, $combo.mode_3, $combo.mode_4) |
        Where-Object { $_ -ne '' } | ForEach-Object { [int]$_ }
    $minimumRaw = [long](($modes | ForEach-Object { Get-RateRawPeriod $_ 2 }) |
        Measure-Object -Sum).Sum
    $maximumRaw = [long](($modes | ForEach-Object { Get-RateRawPeriod $_ 32 }) |
        Measure-Object -Sum).Sum
    $fixedRaw = [long](($modes | ForEach-Object { Get-RateRawPeriod $_ 0 }) |
        Measure-Object -Sum).Sum

    # Multi-rate operation uses the fixed super-frame value 1.
    foreach ($superFrameNum in 1) {
        $candidates = @(Get-Candidates $superFrameNum)
        if ($candidates.Count -eq 0) { continue }
        $maximumCandidate = ($candidates | Measure-Object -Maximum).Maximum
        if ($minimumRaw -gt $maximumCandidate) { continue }

        $minimumSelected = $candidates | Where-Object { $_ -ge $minimumRaw } |
            Sort-Object { $_ - $minimumRaw } | Select-Object -First 1
        $maximumSelected = $candidates | Where-Object { $_ -ge $maximumRaw } |
            Sort-Object { $_ - $maximumRaw } | Select-Object -First 1
        $weightedBudget = $maximumCandidate - $fixedRaw

        [pscustomobject]@{
            rate_count = $combo.rate_count
            base_bcn_mode = $combo.base_bcn_mode
            rate_modes = $combo.rate_modes
            super_frame_num = $superFrameNum
            minimum_blocks_supported = 1
            all_1_to_16_blocks_supported = if ($maximumRaw -le $maximumCandidate) { 1 } else { 0 }
            minimum_raw_period_us = $minimumRaw
            minimum_frame_period_us = $minimumSelected
            minimum_added_gap_us = $minimumSelected - $minimumRaw
            maximum_raw_period_us = $maximumRaw
            maximum_frame_period_us = if ($null -ne $maximumSelected) { $maximumSelected } else { '' }
            maximum_candidate_period_us = $maximumCandidate
            weighted_block_budget_us = $weightedBudget
            block_support_rule = (($modes | ForEach-Object { "$(2L * $body[$_])*(UL$($_)+DL$($_))" }) -join ' + ') + " <= $weightedBudget"
        }
    }
}
$supportRows | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8

# Exact compact traversal. For an R-rate combination, enumerate the UL+DL sums of
# the first R-1 rates and store the complete supported sum range of the last rate.
# Each sum maps losslessly to all UL/DL pairs because both inputs are independently 1..16.
$script:boundaryRowCount = 0
& {
    foreach ($row in $supportRows) {
        $modes = @($row.rate_modes -split '\+' | ForEach-Object { [int]$_ })
        $lastIndex = $modes.Count - 1
        $lastMode = $modes[$lastIndex]
        $lastWeight = 2L * $body[$lastMode]
        $prefixModes = @($modes[0..($lastIndex - 1)])

        # Use a Cartesian counter because every rate has an independent sum.
        $digits = [int[]]::new($prefixModes.Count)
        for ($i = 0; $i -lt $digits.Count; $i++) { $digits[$i] = 2 }
        $finished = $false
        while (-not $finished) {
            $used = 0L
            for ($i = 0; $i -lt $prefixModes.Count; $i++) {
                $used += 2L * $body[$prefixModes[$i]] * $digits[$i]
            }
            $lastMaximumSum = [math]::Min(32, [math]::Floor(($row.weighted_block_budget_us - $used) / $lastWeight))
            if ($lastMaximumSum -ge 2) {
                $values = [ordered]@{
                    rate_count = $row.rate_count
                    rate_modes = $row.rate_modes
                    super_frame_num = 1
                    normal_solution = 1
                    fallback = 0
                    max_cycle_us = 10000000
                }
                $representedCount = 1L
                for ($i = 0; $i -lt 3; $i++) {
                    $position = $i + 1
                    if ($i -lt $prefixModes.Count) {
                        $sum = $digits[$i]
                        $minimumUl = [math]::Max(1, $sum - 16)
                        $maximumUl = [math]::Min(16, $sum - 1)
                        $values["rate${position}_mode"] = $prefixModes[$i]
                        $values["rate${position}_ul_dl_sum"] = $sum
                        $values["rate${position}_ul_range"] = "$minimumUl-$maximumUl"
                        $values["rate${position}_dl_formula"] = "$sum-UL"
                        $representedCount *= $maximumUl - $minimumUl + 1
                    } else {
                        $values["rate${position}_mode"] = ''
                        $values["rate${position}_ul_dl_sum"] = ''
                        $values["rate${position}_ul_range"] = ''
                        $values["rate${position}_dl_formula"] = ''
                    }
                }
                $values['last_rate_mode'] = $lastMode
                $values['last_rate_ul_dl_sum_min'] = 2
                $values['last_rate_ul_dl_sum_max'] = $lastMaximumSum
                $values['last_rate_pair_rule'] = "UL+DL=2..$lastMaximumSum; UL=1..16; DL=1..16"
                $lastRatePairCount = 0L
                for ($sum = 2; $sum -le $lastMaximumSum; $sum++) {
                    $lastRatePairCount += [math]::Min(16, $sum - 1) - [math]::Max(1, $sum - 16) + 1
                }
                $values['supported_config_count'] = $representedCount * $lastRatePairCount
                $values['support_condition'] = 'exists M=1..10,N=1..64: M*1000000%N=0, N%superFrameNum=0, M*1000000/N>=rawPeriod'
                $script:boundaryRowCount++
                [pscustomobject]$values
            }

            for ($digit = $digits.Count - 1; $digit -ge 0; $digit--) {
                if ($digits[$digit] -lt 32) {
                    $digits[$digit]++
                    break
                }
                $digits[$digit] = 2
                if ($digit -eq 0) { $finished = $true }
            }
        }
    }
} | Export-Csv -LiteralPath $supportPath -NoTypeInformation -Encoding UTF8

Write-Output "combinations=$combinationPath rows=$($rateCombinations.Count)"
Write-Output "summary=$summaryPath rows=$($supportRows.Count)"
Write-Output "support=$supportPath boundary_rows=$script:boundaryRowCount"
