function [ModePara] = GetmodelparaGWTxPHY2_1(model,GWTXPara)
% TerminalTxShrink = SysSimulationPara.GWRxShrink;
% GWRXDataWindowNum = SysSimulationPara.GWRxDataWindowNum;
% GWRXBasicDataWindowNum = GWRxPara.RxBasicDataWindowNum;
switch model
    case 11
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 2000;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 4;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 12
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 2000;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 2;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 13
        Mfsk = 4 ;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 2000;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 14
        Mfsk = 4 ;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 4000;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 15
        Mfsk = 4 ;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;%2
        BW = 4000*2;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 16
        Mfsk = 4 ;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;%4
        BW = 4000*4;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 17
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 4000*8;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 18
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 4000*16;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 19
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 4000*32;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
    case 20
        Mfsk = 4 ;
        %         Len_TxByte = 48;
        Len_TxByte = GWTXPara.GWTxByteLen;
        crc14_en = GWTXPara.GWTxCRCEn;
        TxBlockNum = GWTXPara.GWTxBlockNum;
        BW = 4000*32;
        Src_factor = 16;
        CodeRate = 0.75;
        TransRatio = 0.25;
        DSSS = 1;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsTx =  osr * BW * 125/128 ;
end

% GenFskFreqs
[~, ~, FskPhase] = GenFskFreqsTS(Mfsk,Src_factor*4,Src_factor);

% GenPreamble
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

%
load('SCode384.mat');
SCode384 = SCode;

%Para
FskEncodingbitsLen  = Len_TxByte*8*BalanceFactor / 16 ; % Fsk InfoBits Num
DqpskEncodingbitsLen = Len_TxByte*8*(16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
FskSegInfoBitLenSim = ceil(FskEncodingbitsLen - 16 * BalanceFactor / 16);
DqpskSegInfoBitLenSim = floor( DqpskEncodingbitsLen - 16 * (16 - BalanceFactor)/16 );
InfoBitLen = FskSegInfoBitLenSim + DqpskSegInfoBitLenSim;
%
AGCLen = 0;%s
PilotLen = 0.065536;%s
%Out Para
ModePara.PilotLen = PilotLen;
ModePara.AGCLen = AGCLen;
ModePara.Mfsk = Mfsk;
ModePara.Mode = model;
ModePara.Len_TxByte = Len_TxByte;
ModePara.crc14_en = crc14_en;
ModePara.TxBlockNum = TxBlockNum;
ModePara.BW = BW;
ModePara.Src_factor = Src_factor;
ModePara.CodeRate = CodeRate;
ModePara.TransRatio = TransRatio;
ModePara.DSSS = DSSS;
ModePara.BalanceFactor = BalanceFactor;
ModePara.SCode = SCode384;
ModePara.SymNumPreamble = SymNumPreamble;
ModePara.FskPreamble = FskPreamble;
ModePara.PskPreamblePhase = PskPreamblePhase;
ModePara.FskPhase = FskPhase;
ModePara.DSeq = DSeq;
ModePara.InfoBitLen = InfoBitLen;
ModePara.FskEncodingbitsLen = FskEncodingbitsLen;
ModePara.DqpskEncodingbitsLen = DqpskEncodingbitsLen;
ModePara.ExternalCRCLen = ExternalCRCLen;
ModePara.FsTx = FsTx;
end