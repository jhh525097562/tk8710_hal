function [sigTx_slot] = PHY2_1Tx(InfoBitsOneSlot,ModePara)
Mfsk                       = ModePara.Mfsk;
Mode                     = ModePara.Mode;
Len_TxByte                  = ModePara.Len_TxByte;
crc14_en        = ModePara.crc14_en;
TxBlockNum              = ModePara.TxBlockNum;
BW                     = ModePara.BW;
Src_factor               = ModePara.Src_factor;
CodeRate                = ModePara.CodeRate;
TransRatio          = ModePara.TransRatio;
SCode                   = ModePara.SCode;
DSSS                = ModePara.DSSS;
BalanceFactor               = ModePara.BalanceFactor;
SCode               = ModePara.SCode;
SymNumPreamble                      = ModePara.SymNumPreamble ;
FskPreamble                         = ModePara.FskPreamble;    % 卷积码编码器
PskPreamblePhase = ModePara.PskPreamblePhase; % 
FskPhase                      = ModePara.FskPhase ;
DSeq                         = ModePara.DSeq;    % 卷积码编码器
InfoBitLen = ModePara.InfoBitLen; % 
FskEncodingbitsLen = ModePara.FskEncodingbitsLen;
ExternalCRCLen = ModePara.ExternalCRCLen ;

SymNumData = Len_TxByte*8/CodeRate/4;
FskOutLen = SymNumData*2;
DqpskOutLen = SymNumData*2;
sigTxSlotTemp = zeros( Src_factor*(SymNumPreamble + SymNumData)*DSSS,TxBlockNum);
% InputBitsBlockSimSave = zeros(BlockNum,InfoBitLen+16 );
FirstTransiPreamble = [];

for iseg = 1:TxBlockNum
    if Mfsk == 4
        % CRC
        InputBitsBlock = InfoBitsOneSlot((iseg-1)*Len_TxByte*8+1:iseg*Len_TxByte*8);
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
        sigTx  = DpfskModulationTSNew(Mfsk, InterLv_out_fsk, InterLv_out_dqpsk, FskPreamble, PskPreamblePhase, SymNumPreamble, SymNumData, Src_factor, FskPhase, TransRatio, FirstTransiPreamble, iseg, DSeq, DSSS) ;
        % multi-block phase continue
        sigTxSlotTemp(:,iseg) = sigTx(1:(SymNumData+SymNumPreamble)*Src_factor*DSSS,1);
        FirstTransiPreamble = sigTx(end-Src_factor+1:end,1);
%         InputBitsBlockSimSave(iBlock,1:InfoBitLen+16) = InputBitsBlock;
    else

    end
end
sigTx_slot = reshape(sigTxSlotTemp,[],1);
end