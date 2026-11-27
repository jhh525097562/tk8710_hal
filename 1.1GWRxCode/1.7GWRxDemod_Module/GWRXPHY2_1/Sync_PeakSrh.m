function   [correctionTiming,estTimingOffset,EstFreqOffset,EstSyncFreqDrift,avg_power,PeakValue]  = Sync_PeakSrh(corrMaxInT, iTMax, avg_power_tmp, timing_phs_freq, FreqComp, SyncFreqDriftComp, SymNumPreamble, Mfsk)

if size(corrMaxInT,1) > 1
    [PeakValue1,iFreqComp1] = max(corrMaxInT);
    [PeakValue2,iFreqComp2 ] = max(PeakValue1);
    maxrow= iFreqComp1(iFreqComp2); % 对应频漂
    maxcol = iFreqComp2;% 对应频偏
    PeakValue = PeakValue2;
else
    [PeakValue1,iFreqComp1] = max(corrMaxInT);
    maxrow= 1; % 对应频漂
    maxcol = iFreqComp1;% 对应频偏
    PeakValue = PeakValue1;
end

avg_power = avg_power_tmp(maxrow,maxcol)/SymNumPreamble;

% correctionTiming
if Mfsk ==4
tmpPhase = angle(timing_phs_freq(maxrow,maxcol));
timing_phs = tmpPhase/2 ;
correctionTiming = exp(1i*[1/2; 1/6; -1/6; -1/2]*timing_phs);
    
elseif Mfsk==2
    tmpPhase = angle(timing_phs_freq(maxrow,maxcol));
    timing_phs = tmpPhase/2;
    correctionTiming = exp(1i*[1/6; -1/6]*timing_phs);
end

% estTimingOffset
estTimingOffset = iTMax(maxrow,maxcol);

% FreqOffset
EstFreqOffset = FreqComp(1,maxcol);

% SyncFreqDriftComp
EstSyncFreqDrift = SyncFreqDriftComp(maxrow);

end
