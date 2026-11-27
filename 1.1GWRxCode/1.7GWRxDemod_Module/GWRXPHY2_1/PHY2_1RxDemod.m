function  [bitErrorInFrame_o,FSKSNR_o,DQPSKSNR_o,RxData_o,Sbit_o,Fbit_o,estTimingOffset_RegB_o]  =  PHY2_1RxDemod (sigRxIn,ModePara,iBlock,estTimingOffset_RegB_tmp)
% Out: sync_q, sync_timing_offset, sync_freq_offset, sync_Freq_Drift, Data_Freq_Drift，demod_fsk_snr，demod_qpsk_snr，crc_err，Sbit，byte_rx，Data_FDC_Num，Data_FDC_Value
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
sigRx = sigRxIn.';
% filename = sprintf('8610_%d.mat', iii-140+8);     
% load(filename);
% sigRx = test1.';
%% Downsample
% Hd_halfband = tx_halfband_design;
% HBFilter = Hd_halfband.Numerator;
% HalfbandDeciOut = Downsample(sigRx,SysSimulationPara.GWRxSampleRate,ModePara.FsRx,HBFilter);
HalfbandDeciOut = sigRx;
% figure ;plot( phase(HalfbandDeciOut))

%% Rx Filt
% HDRCFilter = FTDesignAstop20;
% RCFilter = HDRCFilter.Numerator;%s 12.0
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

% sync_freq_offset = zeros(1,BlockNum);
% sync_timing_offset = zeros(1,BlockNum);
% sync_Freq_Drift = zeros(1,BlockNum);
% sync_q =  zeros(1,BlockNum);
% crc_err = zeros(1,BlockNum);
% Sbit = zeros(1,BlockNum);
% Data_Freq_Drift = zeros(1,BlockNum);
% demod_fsk_snr = zeros(1,BlockNum);
% demod_qpsk_snr = zeros(1,BlockNum);
% RSSI = zeros(1,BlockNum);

byte_rx = [];
%% for block
% estTimingOffset_RegB_tmp = 1;
tmp_1_4 = round(1/4*length(sigRxFlt));
tmp_3_4 = round(3/4*length(sigRxFlt));
max_IQ = max(abs(real(sigRxFlt(tmp_1_4:tmp_3_4)))) - max(abs(imag(sigRxFlt(tmp_1_4:tmp_3_4))));
if max_IQ > 0
    sigRxFlt = sigRxFlt./max(abs(real(sigRxFlt(tmp_1_4:tmp_3_4)))) ;
else 
    sigRxFlt = sigRxFlt./max(abs(imag(sigRxFlt(tmp_1_4:tmp_3_4)))) ;
end
% for iBlock = 1: BlockNum
    %% Sync
    TempLen = ModePara.LenSym*(ModePara.SymNumPreamble+ModePara.SymNumData)*ModePara.DSSS;
    sigRxFltIn = [sigRxFlt(estTimingOffset_RegB_tmp:end); ones(TempLen*2,1)]; %ones  zeros
    
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
    Sync_New(sigRxFltIn, FskPreamble, PskPreamble, SymNumPreamble, SymNumData, ModePara.LenSym, FskFreqsLoc, DSeq, ModePara.DSSS, FsRx, Mfsk, TOScope, FreqComp, SyncFreqDriftComp,iBlock);
    if iBlock == 1
        estTimingOffset_RegB_tmp = estTimingOffset_RegB;
    else
        estTimingOffset_RegB = estTimingOffset_RegB_tmp;
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
%     sync_freq_offset(iBlock) = estFreqOffset_RegB;
%     sync_timing_offset(iBlock) = estTimingOffset_RegB;
%     sync_Freq_Drift(iBlock) = EstSyncFreqDriftB;
%     sync_q(iBlock) = PeakValue_RegB/avg_power_RegB;
%     byte_rx(iBlock,:) = byte_rxB;
%     crc_err(iBlock) = CRCErrorB;
%     Sbit(iBlock) = SbitB;
%     Fbit(iBlock) = FbitB;
%     Data_Freq_Drift(iBlock) = FreqDriftCompB;
%     demod_fsk_snr(iBlock) = snr_est_fskB;
%     demod_qpsk_snr(iBlock) = snr_est_dqpskB;
%     RSSI(iBlock) = RSSIB;
    
%     for i=1:ModePara.Len_TxByte-2
%         RxData(1,(iBlock-1)*(ModePara.Len_TxByte-2)+i) = byte_rxB(i*8)*1 + byte_rxB(i*8-1)*2 + byte_rxB(i*8-2)*4 + byte_rxB(i*8-3)*8 + byte_rxB(i*8-4)*16 + byte_rxB(i*8-5)*32 + byte_rxB(i*8-6)*64 + byte_rxB(i*8-7)*128;
%     end
    for i=1:ModePara.Len_TxByte-2*ModePara.crc14_en
        RxData(1,i) = byte_rxB(i*8)*1 + byte_rxB(i*8-1)*2 + byte_rxB(i*8-2)*4 + byte_rxB(i*8-3)*8 + byte_rxB(i*8-4)*16 + byte_rxB(i*8-5)*32 + byte_rxB(i*8-6)*64 + byte_rxB(i*8-7)*128;
    end
    %% S F & CRC check
%     if stop_s ==1 && SbitB == 0
%         RcvBlockNum = iBlock;
%         break;
%     else
%         RcvBlockNum = BlockNum;
%     end
%     
%     if stop_CRC ==1 && CRCErrorB == 1
%         RcvBlockNum = iBlock;
%         break;
%     else
%         RcvBlockNum = BlockNum;
%     end

% end

% GWRXDemodParaOut.RcvBlockNum = RcvBlockNum;
% GWRXDemodParaOut.sync_freq_offset = sync_freq_offset;
% GWRXDemodParaOut.sync_timing_offset = sync_timing_offset;
% GWRXDemodParaOut.sync_Freq_Drift = sync_Freq_Drift;
% GWRXDemodParaOut.sync_q = sync_q;
% GWRXDemodParaOut.Sbit = Sbit;
% GWRXDemodParaOut.Fbit = Fbit;
% GWRXDemodParaOut.crc_err = crc_err;
% GWRXDemodParaOut.Data_Freq_Drift = Data_Freq_Drift;
% GWRXDemodParaOut.demod_fsk_snr = demod_fsk_snr;
% GWRXDemodParaOut.demod_qpsk_snr = demod_qpsk_snr;
% GWRXDemodParaOut.RSSI = RSSI;
% GWRXDemodParaOut.RxData = RxData;
bitErrorInFrame_o = CRCErrorB;
FSKSNR_o = snr_est_fskB;
DQPSKSNR_o = snr_est_dqpskB;
RxData_o = RxData;
Sbit_o = SbitB;
Fbit_o = FbitB;
estTimingOffset_RegB_o = estTimingOffset_RegB;
end