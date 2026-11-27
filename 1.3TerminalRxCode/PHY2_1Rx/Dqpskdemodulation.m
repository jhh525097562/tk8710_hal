function  [decoded_out_dqpsk, snr_est, RSSI ]  =  Dqpskdemodulation(decoded_out_fsk,symRxCombine,ReInterLv_out_fsk, avg_qpsk, Mfsk, SymNumData, DqpskaddLen, ModePara)
                                            

% DqpskaddLen = ModePara.DqpskaddLen;
% Mfsk = ModePara.Mfsk;
% SymNumData = ModePara.SymNumData;
DqpskRefBit = [1 0];
SCode = ModePara.SCode;

%% Dqpsk_LLR
% if Mfsk == 4
    dem1h=zeros(SymNumData+1,1);
    dem2h=zeros(SymNumData+1,1);
    dem1h(1)= DqpskRefBit(1);
    dem2h(1)= DqpskRefBit(2);
    dem1h(2:end)=ReInterLv_out_fsk(1:2:end);
    dem2h(2:end)=ReInterLv_out_fsk(2:2:end);
    dem3=zeros(SymNumData,1);
    dem4=zeros(SymNumData,1);
%     tmpE = 0;
    for iSym=1:SymNumData
        tmp1 = Graymapping([],[],dem1h(iSym),dem2h(iSym),Mfsk);
        tmp2 = Graymapping([],[],dem1h(iSym+1),dem2h(iSym+1),Mfsk);
        tmp = (symRxCombine(iSym*Mfsk+tmp2))  * conj(symRxCombine(iSym*Mfsk-Mfsk+tmp1));
%         tmpE = tmpE + abs(symRxCombine(iSym*Mfsk+tmp2))^2;
        tmpMat(iSym) = tmp;%scatterplot(tmpMat);
        tmp3 = Psk4Demodulation(tmp);
        dem3(iSym)=tmp3(1);
        dem4(iSym)=tmp3(2);
    end
    demodBits2LLR = reshape( [dem3 dem4].', 1 , [] );
    demodBits2LLR = demodBits2LLR/avg_qpsk;
    

%% RSSI (取2的N次方个)
RSSI = avg_qpsk;

%% SNRest
tmpMean = mean(abs(demodBits2LLR));
tmpVar = var(abs(demodBits2LLR));
snr_est = 10*log10(tmpMean^2 / tmpVar);

%% Dqpsk_DeInterlv
% DeInterlvBits2LLR = DeInterLv(demodBits2LLR, DqpskInterLvRow, DqpskInterLvCol,SCode);
addLen = DqpskaddLen;
DeInterlvBits2LLR = DeInterLvPunch (demodBits2LLR,addLen,SCode) ;

%% Decode_Recode
SecFlag = 1; % 第二段
[decoded_out_dqpsk,RecodeOut_dqpsk] = Decode_Recode(decoded_out_fsk,DeInterlvBits2LLR,SecFlag);

end