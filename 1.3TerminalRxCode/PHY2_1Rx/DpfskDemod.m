function  [decoded_out_fsk,decoded_out_dqpsk,byte_rx,errorCRC,Sbit,Fbit,DriftPerSec,snr_est_fsk,snr_est_dqpsk, RSSI] = DpfskDemod(sigFltFreqComp,correctionTiming,avg_power,Mfsk,SymNumData,LenSym,FskFreqsLoc,SymNumPreamble,FsRx,DemodFreqDriftComp,ExternalCRCLen,DSeq,DSSSNum,FskaddLen,DqpskaddLen,FskOutLen,DqpskOutLen,ModePara)

% Mfsk = ModePara.Mfsk;
% SymNumData = ModePara.SymNumData;
% LenSym = ModePara.LenSym;
% FskFreqsLoc = ModePara.FskFreqsLoc;
% % FreqDriftCorrectFlag = ModePara.FreqDriftCorrectFlag;
% SymNumPreamble = ModePara.SymNumPreamble;
% FsRx = ModePara.FsRx ;
% DemodFreqDriftComp = ModePara.DemodFreqDriftComp;
% ExternalCRCLen = ModePara.ExternalCRCLen;
% DSeq = ModePara.DSeq;
% DSSSNum = ModePara.DSSSNum ;
% EnNCoFsk = 0;

%% Fsk_Cor_Combine
[symRxCombineDsss]  = Fsk_Cor_Combine(sigFltFreqComp,SymNumData,Mfsk,LenSym,correctionTiming,FskFreqsLoc,DSeq,DSSSNum);%Çófsk·ûºÅÏà¹Ø

%% Freq Drift Commp ÆµÆ®²¹³¥
if DemodFreqDriftComp ~= 0
    symRxCombineDsssFDC = zeros(length(symRxCombineDsss),1);
    Fs = FsRx/(LenSym);
    
    for Iter = 1:length(DemodFreqDriftComp)
        if Iter ==8 
            aa = 1;
        end
        %%  Freq Drift Commp
        DriftPerSec = DemodFreqDriftComp(Iter);
        FreqOffsetDriftTabletmp = 0 : DriftPerSec/Fs : DriftPerSec/Fs * ((SymNumData+SymNumPreamble)*DSSSNum - 1) ;
        tmpPhasedelta = 2* pi.*FreqOffsetDriftTabletmp.*(1/Fs);
        tmpPhase =  zeros(1,length(tmpPhasedelta) );
        tmp = 0;
        for ii = 1:length(tmpPhasedelta)
            tmp = tmp + tmpPhasedelta(ii);
            tmpPhase(ii) = tmp ;
        end
        %       plot(tmpPhase) ; hold on
        tmpExp1 = exp(1j*tmpPhase);
        tmpExp = tmpExp1( (SymNumPreamble-1)*DSSSNum + 1 : end ).';
        
        symRxCombineDsssFDC(1:4:end) = symRxCombineDsss(1:4:end).*tmpExp;
        symRxCombineDsssFDC(2:4:end) = symRxCombineDsss(2:4:end).*tmpExp;
        symRxCombineDsssFDC(3:4:end) = symRxCombineDsss(3:4:end).*tmpExp;
        symRxCombineDsssFDC(4:4:end) = symRxCombineDsss(4:4:end).*tmpExp;
        
        %% Fskdemodulation
        [symRxCombineFDC, Recoded_out_fsk, decoded_out_fsk, avg_p, snr_est_fsk] = NonCoFskdemodulation (symRxCombineDsssFDC, avg_power , SymNumData, Mfsk, FskaddLen, FskOutLen, ModePara, LenSym, DSeq, DSSSNum );

        %% Dqpskdemodulation
        [decoded_out_dqpsk, snr_est_dqpsk, RSSI] = Dqpskdemodulation(decoded_out_fsk,symRxCombineFDC,Recoded_out_fsk, avg_p, Mfsk, SymNumData, DqpskaddLen, ModePara);
        
        %% byte_rx  
         byte_rx =  [decoded_out_fsk decoded_out_dqpsk];
        
        %% Sbit 
         Sbit = byte_rx(end-15);
         Fbit = byte_rx(end-14);
        %% CRC check
        if ModePara.crc14_en ==1
            [errorCRC]= crc_decode(byte_rx,ExternalCRCLen);
        else
            errorCRC = 0;
        end
        decoded_out_dqpsk = decoded_out_dqpsk(1:end-ExternalCRCLen);
        if errorCRC ==0
            break;
        end
        
    end
    
else

     %% Fskdemodulation
     [symRxCombineFDC, Recoded_out_fsk, decoded_out_fsk, avg_p, snr_est_fsk] = NonCoFskdemodulation (symRxCombineDsss, avg_power , SymNumData, Mfsk, FskaddLen, FskOutLen, ModePara, LenSym, DSeq, DSSSNum );
    %% Dqpskdemodulation
    [decoded_out_dqpsk, snr_est_dqpsk, RSSI] = Dqpskdemodulation(decoded_out_fsk,symRxCombineFDC,Recoded_out_fsk, avg_p, Mfsk, SymNumData, DqpskaddLen, ModePara);
    
    %% byte_rx
    byte_rx =  [decoded_out_fsk decoded_out_dqpsk];
    
    %% Sbit
    Sbit = byte_rx(end-15);
    Fbit = byte_rx(end-14);
    %% CRC check
    if ModePara.crc14_en ==1
        [errorCRC]= crc_decode(byte_rx,ExternalCRCLen);
    else
        errorCRC = 0;
    end
    decoded_out_dqpsk = decoded_out_dqpsk(1:end-ExternalCRCLen);
    DriftPerSec = 0;
    
    
end

end