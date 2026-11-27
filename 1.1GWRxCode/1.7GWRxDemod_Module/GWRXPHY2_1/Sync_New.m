function   [ sigFltFreqCompOut, estTimingOffset, estFreqOffset, EstSyncFreqDrift,avg_power, correctionTiming, PeakValue, sync_out_data] =  Sync_New(sigFlt, FskPreamble, PskPreamble, SymNumPreamble, SymNumData, LenSym, FskFreqsLoc, DSeq, DSSSNum, FsRx, Mfsk, TOScope,FreqComp,SyncFreqDriftComp,iBlock)

% Mfsk = ModePara.Mfsk;
% SymNumData = ModePara.SymNumData;
% SymNumPreamble = ModePara.SymNumPreamble;
% LenSym = ModePara.LenSym;
% FsRx = ModePara.FsRx;
% DSSSNum = ModePara.DSSSNum;

% FreqComp = ModePara.FreqComp;
% TOScope = ModePara.TOScope;
% SyncFreqDriftComp = ModePara.SyncFreqDriftComp;
if iBlock ==1
    %%
    [corrMaxInT,iTMax,avg_power_tmp,timing_phs_freq] = Sync_Cor(sigFlt,FreqComp,TOScope,SyncFreqDriftComp,FsRx,FskPreamble,PskPreamble,SymNumPreamble,LenSym, FskFreqsLoc, DSeq, DSSSNum);

    %%
    [correctionTiming,estTimingOffset,estFreqOffset,EstSyncFreqDrift,avg_power,PeakValue] = Sync_PeakSrh(corrMaxInT, iTMax, avg_power_tmp, timing_phs_freq, FreqComp, SyncFreqDriftComp, SymNumPreamble, Mfsk);

    %% freq coarse compensation 
    %EstSyncFreqDrift
    % EstSyncFreqDrift = 0;
    % if EstSyncFreqDrift~=0
    % FreqOffsetDriftTabletmp = 0 : EstSyncFreqDrift/FsRx : EstSyncFreqDrift/FsRx * (length(sigFlt)-1) ;
    % tmpPhasedelta = 2* pi.*FreqOffsetDriftTabletmp.*(1/FsRx);
    % tmpPhase =  zeros(1,length(tmpPhasedelta) );
    % tmp = 0;
    % for ii = 1:length(tmpPhasedelta)
    %     tmp = tmp + tmpPhasedelta(ii);
    %     tmpPhase(ii) = tmp ;
    % end
    % tmpExp = exp(1j*tmpPhase).';
    % else
    % tmpExp = ones(1,length(sigFlt)).';    
    % end

    %estFreqOffset
    tmpI = 0:1:length(sigFlt)-1;
    tmpP = exp(1i*2*pi* estFreqOffset * tmpI/FsRx).';

    %Commp
    sigFltFreqComp = sigFlt.*tmpP;
    sigFltFreqCompSync = sigFltFreqComp( estTimingOffset : (estTimingOffset + (SymNumPreamble + SymNumData) * LenSym * DSSSNum ) -1 );
    sigFltFreqCompOut = sigFltFreqCompSync;
else 
    TOScope = 1;
    [corrMaxInT,iTMax,avg_power_tmp,timing_phs_freq] = Sync_Cor(sigFlt,FreqComp,TOScope,SyncFreqDriftComp,FsRx,FskPreamble,PskPreamble,10,LenSym, FskFreqsLoc, DSeq, DSSSNum);%使用10个符号做sync
    %%
    [correctionTiming,estTimingOffset,estFreqOffset,EstSyncFreqDrift,avg_power,PeakValue] = Sync_PeakSrh(corrMaxInT, iTMax, avg_power_tmp, timing_phs_freq, FreqComp, SyncFreqDriftComp, SymNumPreamble, Mfsk);
    tmpI = 0:1:length(sigFlt)-1;
    tmpP = exp(1i*2*pi* estFreqOffset * tmpI/FsRx).';

    %Commp
    sigFltFreqComp = sigFlt.*tmpP;
        sigFltFreqCompSync = sigFltFreqComp( 1 : (1 + (SymNumPreamble + SymNumData) * LenSym * DSSSNum ) -1 );%estTimingOffset_RegB_tmp
        sigFltFreqCompOut = sigFltFreqCompSync;
end
sync_out_data = 0;
%     sync_out_data = sigFltFreqComp( estTimingOffset : (estTimingOffset + (SymNumPreamble + SymNumData) * LenSym * DSSSNum * seg_num ) -1 );
%     sync_out_data = transpose(reshape(sync_out_data, [], seg_num));
%     sync_out_data(:, 1 : 11 * LenSym * DSSSNum) = [];
end