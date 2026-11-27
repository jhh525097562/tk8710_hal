function  sigTxBlock = sigTxGenTS( InputBitsBlock, ModePara, FirstTransiPreamble, iBlock) %输入：输入比特（整个slot），比特长度，编码器选择，码长，包数，fsk模式，扩频模式    输出：编码比特（整个slot）  for循环包块

FskEncodingbitsLen = ModePara.FskEncodingbitsLen;
FskPreamble = ModePara.FskPreamble;
PskPreamblePhase = ModePara.PskPreamblePhase;
SymNumData = ModePara.SymNumData;
SymNumPreamble = ModePara.SymNumPreamble;
FskPhase = ModePara.FskPhase;
LenSymPhase = ModePara.LenSymPhase ;
Mfsk = ModePara.Mfsk;
ExternalCRCLen = ModePara.ExternalCRCLen ;
TransRatio = ModePara.TransRatio;
FskOutLen = ModePara.FskOutLen;
DqpskOutLen = ModePara.DqpskOutLen;
SCode = ModePara.SCode ;

DSeq = ModePara.DSeq;
DSSSNum = ModePara.DSSSNum ;

%%
% ExternalCRC
[~,crc1] = crc_encode(InputBitsBlock(1:end-ExternalCRCLen),ExternalCRCLen) ;
InputBitsBlock(end-ExternalCRCLen+1:end) = crc1;
% Coding
[enc_out1,enc_out2] = encoding(InputBitsBlock.',FskEncodingbitsLen);
% FSK  InterLvPunch
outLen = FskOutLen;
InterLv_out_fsk = InterLvPunch(enc_out1,outLen,SCode);
% DQPSK InterLvPunch
outLen = DqpskOutLen;
InterLv_out_dqpsk = InterLvPunch(enc_out2,outLen,SCode);
% modulation dpfsk
sigTxBlock  = DpfskModulationTSNew(Mfsk, InterLv_out_fsk, InterLv_out_dqpsk, FskPreamble, PskPreamblePhase, SymNumPreamble, SymNumData, LenSymPhase, FskPhase, TransRatio, FirstTransiPreamble, iBlock,DSeq,DSSSNum) ;

end