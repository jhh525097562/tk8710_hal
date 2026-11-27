function [TerminalRxInf,TerminalRxNum,TempUserID] = PHY2_1TerminalRx(TerminalRxSignal,TMRXPara,TerminalRxFreq,RxMode,GWTxUserNum)

rmpath(genpath('1.1GWRxCode'));
TerminalRxNum = 0;
TempUserID = [];
for ie = 1:GWTxUserNum

    [ModePara] = getmodelparaTMRxPHY2_1(RxMode(ie),TMRXPara);
    %% DownLinkRx
    %% ��Ƶ����Ƶ
    tmpO = 0:1:length(TerminalRxSignal(ie,:))-1;
    IfTx = -TerminalRxFreq(ie); % debug
    TerminalRxSignal0 = TerminalRxSignal(ie,:).*exp(1i*2*pi*tmpO*IfTx/TMRXPara.TMRXSampleRate);
%     [Pxif1, fif1] = pwelch(TerminalRxSignal0,[], [], [],TMRXPara.TMRXSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
    
    % Out: sync_q, sync_timing_offset, sync_freq_offset, sync_Freq_Drift, Data_Freq_Drift��demod_fsk_snr��demod_qpsk_snr��crc_err��Sbit��byte_rx��Data_FDC_Num��Data_FDC_Value
    %% Rx Para Initial
    Mfsk = ModePara.Mfsk;
    FsRx = ModePara.FsRx;
    BlockNum = ModePara.TxBlockNum;
    En_RxFliter = ModePara.En_RxFliter;
    stop_s = ModePara.stop_s;
    stop_CRC = ModePara.stop_CRC;
    
    FskPreamble = ModePara.FskPreamble;
    PskPreamble = ModePara.PskPreamblePhase; %
    SymNumPreamble = ModePara.SymNumPreamble;
    ExternalCRCLen = ModePara.ExternalCRCLen ;
    PayloadBitsNum = ModePara.Len_TxByte * 8;
    CR = ModePara.CodeRate;
    sync_FComp_Num = ModePara.sync_FComp_Num;
    sync_FComp_Delta = ModePara.sync_FComp_Delta;
    sync_FDComp_Num = ModePara.sync_FDComp_Num;
    sync_FDComp_Delta = ModePara.sync_FDComp_Delta;
    data_FDComp_Num = ModePara.data_FDComp_Num;
    data_FDComp_Delta = ModePara.data_FDComp_Delta;
    TOScope = ModePara.TOScope;
    
    if Mfsk == 2
        %     BalanceFactor = 6;
        %     SymNumData =  PayloadBi0tsNum/CR/3 ;
        %     FskEncodingbitsLen  = PayloadBitsNum * BalanceFactor / 16 ; % Fsk InfoBits Num
        %     DqpskEncodingbitsLen = PayloadBitsNum * (16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
        %     %
        %     FskaddLen = FskEncodingbitsLen*2 - SymNumData;  % Patch Len
        %     DqpskaddLen = DqpskEncodingbitsLen*2 - SymNumData*2;  % Patch Len
        %     %
        %     FskOutLen = SymNumData;  % Punch output Len
        %     DqpskOutLen = SymNumData*2;  % Punch output Len
        
    elseif Mfsk == 4
        BalanceFactor = ModePara.BalanceFactor;
        SymNumData =  PayloadBitsNum/CR/4;
        FskEncodingbitsLen  = PayloadBitsNum * BalanceFactor / 16 ; % Fsk InfoBits Num
        DqpskEncodingbitsLen = PayloadBitsNum * (16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
        %
        FskaddLen = FskEncodingbitsLen*2 - SymNumData*2; % Patch Len
        DqpskaddLen = DqpskEncodingbitsLen*2 - SymNumData*2; % Patch Len
        %
        FskOutLen = SymNumData*2; % Punch output Len
        DqpskOutLen = SymNumData*2; % Punch output Len
    end
    
    FskFreqsLoc = ModePara.FskFreqsLoc;
    DSeq = ModePara.DSeq;
    
    % TOScope
%     TOScope = ( ModePara.osr * ModePara.BW  ) * 4/1000 ;
%     if TOScope>256
%         TOScope = 256;
%     end
    
    % FreqComp
    if sync_FComp_Delta == 0
        FreqComp =0;
    else
        Ntemp = sync_FComp_Num;
        DeltaF = sync_FComp_Delta;
        MaxF = DeltaF*(Ntemp-1)/2;
        FreqComp = - MaxF : DeltaF : MaxF ;
    end
    
    % SyncFreqDriftComp
    if sync_FDComp_Delta == 0
        SyncFreqDriftComp =0;
    else
        Ntemp2 = sync_FDComp_Num;
        DeltaF2 = sync_FDComp_Delta;
        MaxF2 = DeltaF2*(Ntemp2-1)/2;
        SyncFreqDriftComp = - MaxF2 : DeltaF2 : MaxF2 ;
    end
    
    % DemodFreqDriftComp
    if data_FDComp_Delta == 0
        DemodFreqDriftComp = 0;
    else
        Ntemp3 = data_FDComp_Num;
        DeltaF3 = data_FDComp_Delta;
        MaxF3 = DeltaF3*(Ntemp3-1)/2;
        DemodFreqDriftComp = - MaxF3 : DeltaF3 : MaxF3;
    end
    
    %% Remove AGC Pilot
    DataPreLen = (ModePara.PilotLen + ModePara.AGCLen)*TMRXPara.TMRXSampleRate;
    m = log2(TMRXPara.TMRXSampleRate/ModePara.FsRx);
    if(RxMode(ie) == 11)
        DataPreLen = 40;
    elseif(RxMode(ie) == 12)
        DataPreLen = 40;
    elseif(RxMode(ie) == 13)
        DataPreLen = 40;
    elseif(RxMode(ie) == 14)
        DataPreLen = 48;
    elseif(RxMode(ie) == 15)
        DataPreLen = 96;
    elseif(RxMode(ie) == 16)
        DataPreLen = 160;
    elseif(RxMode(ie) == 17)
        DataPreLen = 192;
    elseif(RxMode(ie) == 18)
        DataPreLen = 256;
    elseif(RxMode(ie) == 19)
        DataPreLen = 256;
    elseif(RxMode(ie) == 20)
        DataPreLen = 256;
    end
    DataPreLen = DataPreLen * 2^m;
    TerminalRxSignal0 = TerminalRxSignal0(DataPreLen+1:end);
    sigRx = TerminalRxSignal0.';
    % filename = sprintf('8610_%d.mat', iii-140+8);
    % load(filename);
    % sigRx = test1.';
    %% Downsample
%     Hd_halfband = tx_halfband_design;
%     HBFilter = Hd_halfband.Numerator;
    HBFilter = [0,0,0,-0.000976562500000000,0,0.00390625000000000,0,-0.0126953125000000,0,0.0341796875000000,0,-0.0849609375000000,0,0.310546875000000,0.500000000000000,0.310546875000000,0,-0.0849609375000000,0,0.0341796875000000,0,-0.0126953125000000,0,0.00390625000000000,0,-0.000976562500000000,0,0,0];
    HalfbandDeciOut = Downsample(sigRx,TMRXPara.TMRXSampleRate,ModePara.FsRx,HBFilter);
    HalfbandDeciOut = HalfbandDeciOut;
    % figure ;plot( phase(HalfbandDeciOut))
    
    %% Rx Filt
% %     HDRCFilter = FTDesignAstop20;
% %     RCFilter = HDRCFilter.Numerator;%s 12.0
    RCFilter = [0.0103371104398666,0.00400924997649593,-0.0111760251001597,-0.0202209201210192,-0.00340483679782601,0.0276258058944011,0.0300041477320026,-0.0187592457345133,-0.0721712187239547,-0.0420271219462498,0.106351468512823,0.292690143867001,0.377778804946730,0.292690143867001,0.106351468512823,-0.0420271219462498,-0.0721712187239547,-0.0187592457345133,0.0300041477320026,0.0276258058944011,-0.00340483679782601,-0.0202209201210192,-0.0111760251001597,0.00400924997649593,0.0103371104398666];
    FTDelay = ceil(length(RCFilter)/2);
    
    if En_RxFliter == 1
        sigFltTemp = conv(HalfbandDeciOut,RCFilter);
        sigRxFlt = sigFltTemp(FTDelay:end);
    else
        sigRxFlt = HalfbandDeciOut;
    end
    
    %  figure; pwelch(HalfbandDeciOut, [], [],[], ModePara.FsRx);title('spetrum');
    %  figure; pwelch(sigRxFlt, [], [],[], ModePara.FsRx);title('spetrum');
    % figure;  plot(phase(sigRxFlt));
    % figure;  plot(real(sigRxFlt)); figure;  plot(imag(sigRxFlt));
    % figure;  plot(abs(sigRxFlt));
    
    sync_freq_offset = zeros(1,BlockNum);
    sync_timing_offset = zeros(1,BlockNum);
    sync_Freq_Drift = zeros(1,BlockNum);
    % avg_power_RegM = zeros(1,BlockNum);
    % PeakValue_RegM = zeros(1,BlockNum);
    sync_q =  zeros(1,BlockNum);
    crc_err = zeros(1,BlockNum);
    Sbit = zeros(1,BlockNum);
    Data_Freq_Drift = zeros(1,BlockNum);
    demod_fsk_snr = zeros(1,BlockNum);
    demod_qpsk_snr = zeros(1,BlockNum);
    RSSI = zeros(1,BlockNum);
    byte_rx = [];
    %% for block
    estTimingOffset_RegB_tmp = 1;
    tmp_1_4 = round(1/4*length(sigRxFlt));
    tmp_3_4 = round(3/4*length(sigRxFlt));
    max_IQ = max(abs(real(sigRxFlt(tmp_1_4:tmp_3_4)))) - max(abs(imag(sigRxFlt(tmp_1_4:tmp_3_4))));
    if max_IQ > 0
        sigRxFlt = sigRxFlt./max(abs(real(sigRxFlt(tmp_1_4:tmp_3_4)))) ;
    else
        sigRxFlt = sigRxFlt./max(abs(imag(sigRxFlt(tmp_1_4:tmp_3_4)))) ;
    end
    for iBlock = 1: BlockNum
        %% Sync
        TempLen = ModePara.LenSym*(ModePara.SymNumPreamble+ModePara.SymNumData)*ModePara.DSSS;
        sigRxFltIn = [sigRxFlt(estTimingOffset_RegB_tmp+(iBlock-1)*TempLen:end); ones(TempLen*2,1)]; %
        
        for i = 1:length(sigRxFltIn)
            if real(sigRxFltIn(i))>1
                sigRxFltIn(i) = 1+1i*imag(sigRxFltIn(i));
            end
            if imag(sigRxFltIn(i)) >1
                sigRxFltIn(i) = real(sigRxFltIn(i)) + 1i;
            end
            
            if real(sigRxFltIn(i))<-1
                sigRxFltIn(i) = -1 + 1i*imag(sigRxFltIn(i));
            end
            if imag(sigRxFltIn(i)) <-1
                sigRxFltIn(i) = real(sigRxFltIn(i)) + -1i;
            end
        end
        
        [sigFltFreqCompOut, estTimingOffset_RegB, estFreqOffset_RegB, EstSyncFreqDriftB, avg_power_RegB, correctionTiming_Reg, PeakValue_RegB, sync_o] = ...
            Sync_New(sigRxFltIn, FskPreamble, PskPreamble, SymNumPreamble, SymNumData, ModePara.LenSym, FskFreqsLoc, DSeq, ModePara.DSSS, FsRx, Mfsk, TOScope, FreqComp, SyncFreqDriftComp, BlockNum,iBlock,estTimingOffset_RegB_tmp);
        if iBlock == 1
            estTimingOffset_RegB_tmp = estTimingOffset_RegB;
        end
        %     figure;
        %     plot(phase(sigFltFreqCompOut))
        %% Demod
        sigFltFreqCompOut = sigFltFreqCompOut(1+(ModePara.SymNumPreamble - 1)*ModePara.LenSym*ModePara.DSSS:end,1);
        sigFltFreqCompOutDemod = sigFltFreqCompOut;
        
        %sigFltFreqCompOutDemod = 0.8*sigFltFreqCompOutDemod;
        [decoded_out_fskSim,decoded_out_dqpskSim,byte_rxB,CRCErrorB,SbitB,FbitB,FreqDriftCompB,snr_est_fskB,snr_est_dqpskB,RSSIB] = ...
            DpfskDemod(sigFltFreqCompOutDemod,correctionTiming_Reg,avg_power_RegB,Mfsk,SymNumData,ModePara.LenSym,FskFreqsLoc,SymNumPreamble,FsRx,DemodFreqDriftComp,ExternalCRCLen,DSeq,ModePara.DSSS,FskaddLen,DqpskaddLen,FskOutLen,DqpskOutLen,ModePara);
        %% ERR CNT Sim
        sync_freq_offset(iBlock) = estFreqOffset_RegB;
        sync_timing_offset(iBlock) = estTimingOffset_RegB;
        sync_Freq_Drift(iBlock) = EstSyncFreqDriftB;
        sync_q(iBlock) = PeakValue_RegB/avg_power_RegB;
        byte_rx(iBlock,:) = byte_rxB;
        crc_err(iBlock) = CRCErrorB;
        Sbit(iBlock) = SbitB;
        Fbit(iBlock) = FbitB;
        Data_Freq_Drift(iBlock) = FreqDriftCompB;
        demod_fsk_snr(iBlock) = snr_est_fskB;
        demod_qpsk_snr(iBlock) = snr_est_dqpskB;
        RSSI(iBlock) = RSSIB;
        
        for i=1:ModePara.Len_TxByte-2*ModePara.crc14_en
            RxData(1,(iBlock-1)*(ModePara.Len_TxByte-2*ModePara.crc14_en)+i) = byte_rxB(i*8)*1 + byte_rxB(i*8-1)*2 + byte_rxB(i*8-2)*4 + byte_rxB(i*8-3)*8 + byte_rxB(i*8-4)*16 + byte_rxB(i*8-5)*32 + byte_rxB(i*8-6)*64 + byte_rxB(i*8-7)*128;
        end
        %% S F & CRC check
        if stop_s ==1 && SbitB == 0
            RcvBlockNum = iBlock;
            break;
        else
            RcvBlockNum = BlockNum;
        end
        
        if stop_CRC ==1 && CRCErrorB == 1
            RcvBlockNum = iBlock;
            break;
        else
            RcvBlockNum = BlockNum;
        end
        
    end
    if sum(crc_err)==0
        TerminalRxNum = TerminalRxNum + 1;
        TerminalRxInf(TerminalRxNum).SnrFsk_dqpsk = [mean(demod_fsk_snr) mean(demod_qpsk_snr)];
        TerminalRxInf(TerminalRxNum).CRC = sum(crc_err);
        TerminalRxInf(TerminalRxNum).Data = RxData(1:BlockNum*(ModePara.Len_TxByte-2*ModePara.crc14_en));
        TerminalRxInf(TerminalRxNum).RxMode = RxMode(ie);
        TerminalRxInf(TerminalRxNum).UserID = RxData(1,1:2);
        TerminalRxInf(TerminalRxNum).Sbit = Sbit;
        TerminalRxInf(TerminalRxNum).Fbit = Fbit;
        TempUserID(TerminalRxNum) = TerminalRxInf(TerminalRxNum).UserID(1)*255+TerminalRxInf(TerminalRxNum).UserID(2);
    end

end
TerminalRxNum
if TerminalRxNum>=1
    
else
    TerminalRxInf = [];
end
addpath(genpath('1.1GWRxCode'));
end