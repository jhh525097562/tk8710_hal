function  [corrMaxInT , iTMax, avg_power_tmp, timing_phs_freq ] = Sync_Cor(sigFlt,FreqComp,TOScope,SyncFreqDriftComp,FsRx,FskPreamble,PskPreamble,SymNumPreamble,LenSym, FskFreqsLoc, DSeq, DSSSNum)

% PskPreamble = ModePara.PskPreamble;
% SymNumPreamble = ModePara.SymNumPreamble;
% LenSym = ModePara.LenSym;
% FskFreqsLoc = ModePara.FskFreqsLoc;
% FskPreamble = ModePara.FskPreamble;
% 
% DSeq = ModePara.DSeq;
% DSSSNum = ModePara.DSSSNum ;

symRx=zeros(SymNumPreamble,1);
corrMat_coh = zeros(TOScope,length(FreqComp));
corrMat_nocoh = zeros(TOScope,length(FreqComp));
timing_phs_tmp = zeros(TOScope,length(FreqComp));

corrMaxInT = zeros(length(SyncFreqDriftComp),length(FreqComp));
iTMax = zeros(length(SyncFreqDriftComp),length(FreqComp));
avg_power_tmp = zeros(length(SyncFreqDriftComp), length(FreqComp));
timing_phs_freq = zeros(length(SyncFreqDriftComp), length(FreqComp));

for iFD = 1: length(SyncFreqDriftComp)
    
    DriftPerSec = SyncFreqDriftComp(iFD);
    if DriftPerSec~=0
        FreqOffsetDriftTabletmp = 0 : DriftPerSec/FsRx : DriftPerSec/FsRx * ( (LenSym*SymNumPreamble)*DSSSNum - 1 + TOScope  ) ;
        tmpPhasedelta = 2* pi.*FreqOffsetDriftTabletmp.*(1/FsRx);
        tmpPhase =  zeros(1,length(tmpPhasedelta) );
        tmp = 0;
        for ii = 1:length(tmpPhasedelta)
            tmp = tmp + tmpPhasedelta(ii);
            tmpPhase(ii) = tmp ;
        end
        tmpExp = exp(1j*tmpPhase);

    else
        tmpExp =  ones(1,LenSym*SymNumPreamble*DSSSNum + TOScope);
    end
    
    for iFO = 1:length(FreqComp)
        
        % freq pre compensation
        tmpI = 0:1:LenSym*SymNumPreamble*DSSSNum-1+TOScope;
%         tmpI = 0:1:LenSym*SymNumPreamble*DSSSNum-1;
        tmpP = exp(1i*2*pi*FreqComp(1,iFO)*tmpI/FsRx);
%         tmpPM = reshape(tmpP,[],SymNumPreamble);
        
        sigFlt_Commp = sigFlt(1:LenSym*SymNumPreamble*DSSSNum+TOScope).*(tmpP.').*(tmpExp.');
        
        for iTO = 1:TOScope
            for iSym = 1:SymNumPreamble  % 用当前timing offset解调
%                 tmpDS = sigFlt((iSym-1)*LenSym*DSSSNum+iTO : iSym*LenSym*DSSSNum+iTO-1).*tmpExpM(:,iSym).*tmpPM(:,iSym);
                tmpDS = sigFlt_Commp((iSym-1)*LenSym*DSSSNum+iTO : iSym*LenSym*DSSSNum+iTO-1);
                tmpDSR =  reshape(tmpDS, LenSym, []).';
                tmpDSR1 = tmpDSR.*DSeq.';
                tmpS = sum(tmpDSR1,1).'/ DSSSNum ;
                symRx(iSym) = FskFreqsLoc(FskPreamble(iSym),:)*tmpS*conj(PskPreamble(iSym))/LenSym;
            end
            
            % timing_phs_freq
            symRx_tmp1 = symRx;
            symRx_tmp_diff = symRx_tmp1(2:SymNumPreamble-1) .* conj(symRx_tmp1(1:SymNumPreamble-2));
            sum_odd = sum(symRx_tmp_diff(1:2:end));
            sum_even = sum(symRx_tmp_diff(2:2:end));
            timing_phs_tmp(iTO,iFO) = sum_odd .* conj(sum_even);
            
            %
            tmpCorr2 = sum( abs(symRx).^2 );
            corrMat_nocoh(iTO,iFO)= tmpCorr2;
            tmpCorr = sum(symRx);
            corrMat_coh(iTO,iFO)= abs(tmpCorr).^2;
            corrMat = corrMat_coh;
        end
        
        [tmpM,tmpIT]=max(corrMat(:,iFO));  % 在时间上找最大
        
        %         corrMaxInT(1,iFO) = tmpM;
        %         iTMax(1,iFO) = tmpIT;
        %         avg_power_tmp(1, iFO) = corrMat_nocoh(tmpIT, iFO);
        %         timing_phs_freq(1, iFO) = timing_phs_tmp(tmpIT, iFO);
        
        corrMaxInT(iFD,iFO) = tmpM;
        iTMax(iFD,iFO) = tmpIT;
        avg_power_tmp(iFD, iFO) = corrMat_nocoh(tmpIT, iFO);
        timing_phs_freq(iFD, iFO) = timing_phs_tmp(tmpIT, iFO);
    end
    
end

% figure;mesh(corrMat)

end