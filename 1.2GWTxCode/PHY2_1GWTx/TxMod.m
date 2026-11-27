function [sigTxUpsampleOut,InputBitsBlockSimSave,sigTxphase] =  TxMod(byte_tx,len_tx,crc14_en,BlockNum,BW,src_factor,CR,TransRatio,DSSS,ModePara,SamplingRateTx,PilotLen,AGCLen)
global iii;
global txt;
Mfsk= 4;
LenSymPhase = src_factor;  % src : src_factor
LenSym = 16 * Mfsk ; 
ExternalCRCLen = 14 ;
SCode = ModePara.SCode ;
% SymbolRate = BW/Mfsk;
% osr = 64;
osr = 4;
FsTx =  osr * BW * 125/128 ;

%% Initial
PayloadBitsNum = len_tx * 8;
if Mfsk == 2
    BalanceFactor = 6;
    SymNumData = PayloadBitsNum/CR/3;
    FskEncodingbitsLen  = PayloadBitsNum * BalanceFactor / 16 ; % Fsk InfoBits Num
    DqpskEncodingbitsLen = PayloadBitsNum * (16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
    %
    FskSegInfoBitLenSim = (FskEncodingbitsLen - 16 * BalanceFactor / 16);
    DqpskSegInfoBitLenSim = ( DqpskEncodingbitsLen - 16 * (16 - BalanceFactor)/16 );
    InfoBitLen = FskSegInfoBitLenSim + DqpskSegInfoBitLenSim;
    %
    FskOutLen = SymNumData;  % Punch output Len
    DqpskOutLen = SymNumData*2;  % Punch output Len
elseif Mfsk == 4
    BalanceFactor = 8;
    SymNumData = PayloadBitsNum/CR/4;
    FskEncodingbitsLen  = PayloadBitsNum * BalanceFactor / 16 ; % Fsk InfoBits Num
    DqpskEncodingbitsLen = PayloadBitsNum * (16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
    %
    FskSegInfoBitLenSim = ( FskEncodingbitsLen - 16 * BalanceFactor / 16);
    DqpskSegInfoBitLenSim = ( DqpskEncodingbitsLen - 16 * (16 - BalanceFactor)/16 );
    InfoBitLen = FskSegInfoBitLenSim + DqpskSegInfoBitLenSim;
    %
    FskOutLen = SymNumData*2; % Punch output Len
    DqpskOutLen = SymNumData*2; % Punch output Len
end

% GenFskFreqs
[~, ~, FskPhase] = GenFskFreqsTS(Mfsk,LenSym,LenSymPhase);

% GenPreamble
SymNumPreamble = 12; %  Len preamble
FskPreamble = [1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk];
PskPreamblePhase = [pi  pi  0  pi  0  0  pi  0  0  pi  pi pi];%[3.1416  3.1416  0  3.1416  0  0  3.1416  0  0  3.1416  3.1416 3.1416];

% DSeq
switch  DSSS
    case 32
        DSeq = [1,1,1,1,-1,1,-1,1,-1,-1,-1,1,-1,-1,1,1,1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,1,1,-1,1];% 32
    case 16
        DSeq = [1,1,1,-1,-1,-1,-1,1,-1,1,-1,-1,1,1,-1,1]; % 16
    case 8
        DSeq = [1,1,-1,-1,1,1,-1,-1];% 8 
    case 4
        DSeq = [1,1,-1,-1];% 4
    case 2
        DSeq = [1,1];% 2
    case 1
        DSeq = 1;% 1
end

%% sigTx
sigTxSlotTemp = zeros( LenSymPhase*(SymNumPreamble + SymNumData)*DSSS,BlockNum);
InputBitsBlockSimSave = zeros(BlockNum,InfoBitLen+16 );
FirstTransiPreamble = [];
for iBlock = 1: BlockNum
    % CRC
    InputBitsBlock = byte_tx(iBlock,:).';
    if crc14_en == 1
%         InputBitsBlock = byte_tx(iBlock,:).';
        [~,crc1] = crc_encode(InputBitsBlock(1:end-ExternalCRCLen),ExternalCRCLen) ;
        InputBitsBlock(end-ExternalCRCLen+1:end) = crc1;
    end
    % Coding
    enc_out_fsk = CCEncoder(InputBitsBlock(1:FskEncodingbitsLen).');
    enc_out_dqpsk = CCEncoder(InputBitsBlock(FskEncodingbitsLen+1:end).');
    % InterLvPunch
    InterLv_out_fsk = InterLvPunch(enc_out_fsk,FskOutLen,SCode);
    InterLv_out_dqpsk = InterLvPunch(enc_out_dqpsk,DqpskOutLen,SCode);
    % modulation dpfsk
    sigTx  = DpfskModulationTSNew(Mfsk, InterLv_out_fsk, InterLv_out_dqpsk, FskPreamble, PskPreamblePhase, SymNumPreamble, SymNumData, LenSymPhase, FskPhase, TransRatio, FirstTransiPreamble, iBlock, DSeq, DSSS) ;
    % multi-block phase continue
    sigTxSlotTemp(:,iBlock) = sigTx(1:(SymNumData+SymNumPreamble)*LenSymPhase*DSSS,1);
    FirstTransiPreamble = sigTx(end-LenSymPhase+1:end,1);
    %
    InputBitsBlockSimSave(iBlock,1:InfoBitLen+16) = InputBitsBlock;
 
end

sigTxSlot = reshape(sigTxSlotTemp,[],1);
% sigTxPhase = mod(sigTxSlot,6*pi);
% sigTxPhase(sigTxPhase > 3*pi) = sigTxPhase(sigTxPhase > 3*pi) - 6*pi;
% sigTxPhase = sigTxPhase / 3 / pi;
% % %  t_phase_tmp = round((sigTxPhase).*2^9)./2^9;%on or off
% % %  t_phase_tmp(t_phase_tmp==1) = 1-1/512;
% % %   t_phase_tmp(t_phase_tmp==-1) = -1+1/512;
% sigTxSlot =sigTxPhase;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%test mod%%%%%%%%%%%%%%%%%%%%%%%%%
    txt(iii,1) = len_tx;
    txt(iii,2) = CR;
    txt(iii,3) = BlockNum;
    txt(iii,4) = crc14_en;
    txt(iii,5) = length(InterLv_out_fsk);
    txt(iii,6) = SymNumData;
    txt(iii,7) = LenSymPhase;
    txt(iii,8) = TransRatio;
    txt(iii,9) = DSSS;
    txt(iii,10) = BlockNum;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% upsample
m = log2(SamplingRateTx/FsTx);
out1 = upsampling_new(sigTxSlot,m);

% AGC Pilot
% PilotLen = 0;
% AGCLen = 0;
% PilotPhase = zeros(1,SamplingRateTx*PilotLen*src_factor/256)-1;
% AGCPhase = zeros(1,SamplingRateTx*AGCLen*src_factor/256)-1;

sigTxUpsample = [-1*ones(1,16384*SamplingRateTx/1000000) out1];
% sigTxUpsample = [PilotPhase AGCPhase out1];
% test1 = [ones(1,32*4096)*-1 out1];
% filename = sprintf('data_%d', iii-116);
% save(filename,'test1');

% pll set SamplingRateTx = 32MHz
if 0
    fref = SamplingRateTx;
    SDM_EN = 1;
    delta_kvco = 0.00;
    mod_en = 1;
    [f_out] = pll_model_new(fref,SDM_EN,delta_kvco,mod_en,sigTxUpsample);
    sigTxUpsample = f_out;
end

sigTxUpsampleOut = exp(1*pi*1j*sigTxUpsample);
sigTxUpsampleOut = sigTxUpsampleOut(1:end-3);
for ck0 = 1 : 1 : length(sigTxUpsample) - 1
    if(sigTxUpsample(ck0 + 1) - sigTxUpsample(ck0) > 1)
        sigTxphase(ck0) = sigTxUpsample(ck0 + 1) - (sigTxUpsample(ck0) + 2);
    elseif(sigTxUpsample(ck0) - sigTxUpsample(ck0 + 1)  > 1)
        sigTxphase(ck0) = sigTxUpsample(ck0 + 1) - (sigTxUpsample(ck0) - 2);
    else
        sigTxphase(ck0) = sigTxUpsample(ck0 + 1) - sigTxUpsample(ck0);
    end 
end
% sigTxphase = diff(sigTxUpsample);
sigTxphase = [0, sigTxphase * 1 * pi];

% figure; pwelch(sigTxUpsample, [], [],[], SamplingRateTx);title('spetrum');
% figure; pwelch(exp(1j*sigTx), [], [],[], ModePara.SymbolRate*64);title('spetrum');
% plot(abs(sigTxUpsample)) 
% figure; plot((sigTxSlotTemp))
end