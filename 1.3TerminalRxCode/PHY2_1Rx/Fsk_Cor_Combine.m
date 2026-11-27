function  [symRxCombineDsss]  = Fsk_Cor_Combine(sigFltFreqComp,SymNumDataCombine,Mfsk,LenSym,correctionTiming,FskFreqsLoc,DSeq,DSSSNum)

%Cor_Combine
symRxCombineDsss = zeros((SymNumDataCombine+1)*Mfsk*DSSSNum,1);
% sigFTO = sigFltFreqComp(1+LenSym*DSSSNum:end);

% Dqpsk µÄref
% tmpDS = sigFltFreqComp(1:LenSym*DSSSNum);
% tmpDSR =  reshape(tmpDS, LenSym, []).';
% tmpDSR1 = tmpDSR.*DSeq.';
% tmpDSR1Sum = sum(tmpDSR1,1).'/ DSSSNum;
% plot(phase(tmpDS))
% plot(phase(tmpDSR1Sum))

% Ref = FskFreqsLoc*tmpDSR1Sum/LenSym;
% symRxCombineDsss(1:Mfsk) = Ref.*correctionTiming;

% demod
for iSym=1:(SymNumDataCombine+1)*DSSSNum
%     tmpDS = sigFltFreqComp((iSym-1)*LenSym*DSSSNum+1:iSym*LenSym*DSSSNum);
%     tmpDSR =  reshape( tmpDS, LenSym, []).';
%     tmpDSR1 = tmpDSR.*DSeq.';
%     tmpDSR1Sum = sum(tmpDSR1,1).'/ DSSSNum;
    
    tmpS = sigFltFreqComp((iSym-1)*LenSym+1:(iSym-1)*LenSym+LenSym);
%     tmpS = tmpDSR1Sum;
    tmpC = FskFreqsLoc*tmpS/LenSym.*correctionTiming;
    symRxCombineDsss( (iSym-1)*Mfsk+1:(iSym)*Mfsk ) = tmpC;
%     symRxCombine( (iSym)*Mfsk+1:(iSym)*Mfsk + Mfsk ) = symRxCombine( (iSym)*Mfsk+1:(iSym)*Mfsk + Mfsk );
end

end
