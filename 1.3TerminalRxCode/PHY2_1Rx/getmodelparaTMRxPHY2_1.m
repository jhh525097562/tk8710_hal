function [ModePara] = getmodelparaTMRxPHY2_1(model,TMRXPara)
switch model
    
    case 11
        Mfsk = 4 ;
        DSSS = 4;
        BW = 500*DSSS;
        SymbolRate = BW/DSSS/Mfsk*125/128/1000;
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 32;
        sync_FComp_Delta = 5;
        sync_FDComp_Num = 2;
        sync_FDComp_Delta = 30;
        data_FDComp_Num = 8;
        data_FDComp_Delta = 8;
        TOScope = ( osr * BW  ) * 4/1000;
    case 12
        Mfsk = 4 ;
        DSSS = 2;
        BW = 1000*DSSS;
        SymbolRate = BW/DSSS/Mfsk*125/128/1000;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
        BW = 2000;
        Src_factor = 16;
        CodeRate = 0.5;
        TransRatio = 0.25;
        BalanceFactor = 8;
        ExternalCRCLen = 14;
        %         SymNumPreamble = 12; %  Len preamble
        SymNumPreamble = 12; %  Len preamble
        osr = 4;
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 64;
        sync_FComp_Delta = 10;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 8;
        data_FDComp_Delta = 50;
        TOScope = ( osr * BW  ) * 4/1000;
    case 13
        Mfsk = 4 ;
        SymbolRate = 125/128*0.5;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 300;%300 150 100
        TOScope = ( osr * BW  ) * 4/1000;
        %         data_FDComp_Num = 2;
        %         data_FDComp_Delta = 300;
    case 14
        Mfsk = 4 ;
        SymbolRate = 125/128*1;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 300;
        TOScope = ( osr * BW  ) * 4/1000;
    case 15
        Mfsk = 4 ;
        SymbolRate = 125/128*2;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;%2
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 150;
        TOScope = ( osr * BW  ) * 4/1000;
    case 16
        Mfsk = 4 ;
        SymbolRate = 125/128*4;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;%4
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 300;
        TOScope = ( osr * BW  ) * 4/1000;
    case 17
        Mfsk = 4 ;
        SymbolRate = 125/128*8;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 0;
        TOScope = ( osr * BW  ) * 2/1000;
    case 18
        Mfsk = 4 ;
        SymbolRate = 125/128*16;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 4;
        data_FDComp_Delta = 0;%300 150 100
        TOScope = ( osr * BW  ) * 2/1000;
    case 19
        Mfsk = 4 ;
        SymbolRate = 125/128*32;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 300;%300 150 100
        TOScope = ( osr * BW  ) * 1/1000;
    case 20
        Mfsk = 4 ;
        SymbolRate = 125/128*32;
        %         Fs = 16000* SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        Len_TxByte = TMRXPara.TMRxByteLen;
        crc14_en = TMRXPara.TMRxCRCEn;
        TxBlockNum = TMRXPara.TMRxBlockNum;
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
        FsRx =  osr * BW * 125/128 ;
        %RX
        stop_s = 0;
        stop_CRC = 0;
        En_RxFliter = 1;
        sync_FComp_Num = 16;
        sync_FComp_Delta = 40;
        sync_FDComp_Num = 0;
        sync_FDComp_Delta = 0;
        data_FDComp_Num = 2;
        data_FDComp_Delta = 300;%300 150 100
        TOScope = ( osr * BW  ) * 1/1000;        
end

% GenFskFreqs
LenSym = 4 * Mfsk; % Rx Fsk Pattern
LenSymPhase = 64 * Mfsk ;
[~, FskFreqsLoc, ~] = GenFskFreqsTS(Mfsk,LenSym,LenSymPhase);

% GenPreamble
FskPreamble = [1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk;1;Mfsk];
PskPreamble = [-1  -1   1  -1   1   1  -1   1   1  -1  -1  -1].'; %

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
SymNumData =  Len_TxByte*8/CodeRate/4;
FskEncodingbitsLen  = Len_TxByte*8*BalanceFactor / 16 ; % Fsk InfoBits Num
DqpskEncodingbitsLen = Len_TxByte*8*(16 - BalanceFactor)/16 ; % Dqpsk InfoBits Num
FskSegInfoBitLenSim = ceil(FskEncodingbitsLen - 16 * BalanceFactor / 16);
DqpskSegInfoBitLenSim = floor( DqpskEncodingbitsLen - 16 * (16 - BalanceFactor)/16 );
InfoBitLen = FskSegInfoBitLenSim + DqpskSegInfoBitLenSim;
%
AGCLen = 0;%s
PilotLen = 0.065536*1;%s
%Out Para
ModePara.PilotLen = PilotLen;
ModePara.AGCLen = AGCLen;
ModePara.Mfsk = Mfsk;
ModePara.SymbolRate = SymbolRate;
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
ModePara.SymNumData = SymNumData;
ModePara.FskPreamble = FskPreamble;
ModePara.PskPreamblePhase = PskPreamble;
ModePara.FskFreqsLoc = FskFreqsLoc;
ModePara.DSeq = DSeq;
ModePara.InfoBitLen = InfoBitLen;
ModePara.FskEncodingbitsLen = FskEncodingbitsLen;
ModePara.DqpskEncodingbitsLen = DqpskEncodingbitsLen;
ModePara.ExternalCRCLen = ExternalCRCLen;
ModePara.FsRx = FsRx;
ModePara.osr = osr;
ModePara.LenSym = LenSym;

%Rx
ModePara.stop_s = stop_s;
ModePara.stop_CRC = stop_CRC;
ModePara.En_RxFliter = En_RxFliter;
ModePara.sync_FComp_Num = sync_FComp_Num;
ModePara.sync_FComp_Delta = sync_FComp_Delta;
ModePara.sync_FDComp_Num = sync_FDComp_Num;
ModePara.sync_FDComp_Delta = sync_FDComp_Delta;
ModePara.data_FDComp_Num = data_FDComp_Num;
ModePara.data_FDComp_Delta = data_FDComp_Delta;
ModePara.TOScope = TOScope;
end