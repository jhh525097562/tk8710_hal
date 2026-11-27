function [ symRxCombine, ReInterLv_out_fsk, decoded_out_fsk, avg_qpsk, snr_est]  =  NonCoFskdemodulation (symRxCombineDsssFDC, avg_power , SymNumData, Mfsk, FskaddLen, FskOutLen, ModePara , LenSym, DSeq, DSSSNum )

SCode = ModePara.SCode;

%% De Dsss ½âÀ©
symRxCombine = zeros((SymNumData+1)*Mfsk,1);
for iSym=1: SymNumData+1
    tmpDS = symRxCombineDsssFDC((iSym-1)*DSSSNum*Mfsk+1:iSym*DSSSNum*Mfsk);
    tmpDSR =  reshape( tmpDS, Mfsk, []).';
    tmpDSR1 = tmpDSR.*DSeq.';
    tmpDSR1Sum = sum(tmpDSR1,1).'/ DSSSNum ;
    symRxCombine( (iSym-1)*Mfsk+1:iSym*Mfsk ) = tmpDSR1Sum;
end

%% Fsk LLR
[demodBits1LLR,avg_qpsk] = Fsk4_LLR(symRxCombine,SymNumData,avg_power,Mfsk); % Fsk4_LLR
% scatterplot( demodBits1LLR )

%% DeInterlv
% DeInterlvBits1LLR = DeInterLv( demodBits1LLR, FskInterLvRow, FskInterLvCol, SCode);
addLen = FskaddLen;
DeInterlvBits1LLR = DeInterLvPunch (demodBits1LLR,addLen,SCode) ;

%% Decode_Recode
SecFlag = 0; % µÚ1¶Î
[decoded_out_fsk,RecodeOut_fsk] = Decode_Recode([],DeInterlvBits1LLR, SecFlag);

%% Interlv
outLen = FskOutLen;
ReInterLv_out_fsk = InterLvPunch(RecodeOut_fsk,outLen,SCode);

%% SNRest
tmpMean = mean( demodBits1LLR .* (1 - 2*ReInterLv_out_fsk) );
tmpVar = var( demodBits1LLR .* (1 - 2*ReInterLv_out_fsk)  );
snr_est = 10*log10(tmpMean^2 / tmpVar);

end

