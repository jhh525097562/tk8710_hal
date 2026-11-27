function  [paraRxPHY] = getmodelparaRxPHY(model,SysSimulationPara,GWRxPara)

GWRxShrink = SysSimulationPara.GWRxShrink;
GWRXDataWindowNum = SysSimulationPara.GWRxDataWindowNum;
GWRXPilotWindowNum = SysSimulationPara.GWRxPilotWindowNum;
GWRXBasicDataWindowNum = GWRxPara.RxBasicDataWindowNum;
CoderConstraint = 7;
Tre = poly2trellis(CoderConstraint, [171, 133]);    % 卷积码编码器
Mod4Psk = comm.PSKModulator('ModulationOrder',4,'PhaseOffset',0,'BitInput',true,'SymbolMapping','Gray');
Demod4Psk = comm.PSKDemodulator('ModulationOrder',4,'PhaseOffset',0,'BitOutput',true,...
    'DecisionMethod','Log-likelihood ratio','VarianceSource','Property','SymbolMapping','Gray');...
    
Mod8Psk = comm.PSKModulator('ModulationOrder',8,'PhaseOffset',0,'BitInput',true,'SymbolMapping','Gray');
Demod8Psk = comm.PSKDemodulator('ModulationOrder',8,'PhaseOffset',0,'BitOutput',true,...
    'DecisionMethod','Approximate Log-likelihood ratio','VarianceSource','Property','SymbolMapping','Gray');...
    
ScrambleSeq = GenScrambleSeq([1 0 1 0 1 1 0 1 1]);%9阶m序列生成 ZH
ScrambleSeq = [ScrambleSeq 0];
SCode = ScrambleSeq;
SCode = [SCode SCode];

poly1 = [ 1     1     0     0     0     0        1];
init_state1 = [1 0 0 0 1 0];
[PN64] = pnGen(poly1, init_state1, 64); % generate enough length
PN64_s = symbolMapping(PN64, 'BPSK');
PN_s = PN64_s(1:16);


paraRxPHY.Mode = model;
paraRxPHY.Tre = Tre;
paraRxPHY.SCode = SCode;
paraRxPHY.PN_s = PN_s;
paraRxPHY.Mod4Psk = Mod4Psk;
paraRxPHY.Demod4Psk = Demod4Psk;
paraRxPHY.Mod8Psk = Mod8Psk;
paraRxPHY.Demod8Psk = Demod8Psk;

switch model
    case 19
        paraRxPHY.Mfsk = 4;
        paraRxPHY.SymbolRate = 125/128;
        paraRxPHY.Fs = 16000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 113;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 240;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 252;
        paraRxPHY.LenN = 226;
        paraRxPHY.NumByte = 28;
        paraRxPHY.SegNum = 1*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum)*GWRxShrink;%1个包块对应2个window
        paraRxPHY.SymNumPN = 9;
    case 20
        paraRxPHY.Mfsk = 4;
        paraRxPHY.SymbolRate = 125/64;
        paraRxPHY.Fs = 16000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 113;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 240;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 252;
        paraRxPHY.LenN = 226;
        paraRxPHY.NumByte = 28;
        paraRxPHY.SegNum = 1*GWRXDataWindowNum*GWRxShrink;%2个包块对应2个window
        paraRxPHY.SymNumPN = 9;
    case 21
        paraRxPHY.Mfsk = 4;
        paraRxPHY.SymbolRate = 125/32;
        paraRxPHY.Fs = 16000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 113;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 240;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 252;
        paraRxPHY.LenN = 226;
        paraRxPHY.NumByte = 28;
        paraRxPHY.SegNum = 2*GWRXDataWindowNum*GWRxShrink;%4个包块对应2个window
        paraRxPHY.SymNumPN = 9;
    case 22
        paraRxPHY.Mfsk = 4;
        paraRxPHY.SymbolRate = 125/16;
        paraRxPHY.Fs = 16000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 113;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 240;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 252;
        paraRxPHY.LenN = 226;
        paraRxPHY.NumByte = 28;
        paraRxPHY.SegNum = 4*GWRXDataWindowNum*GWRxShrink;%8个包块对应2个window
        paraRxPHY.SymNumPN = 9;
    case 23
        paraRxPHY.Mfsk = 0;
        paraRxPHY.SymbolRate = 125/64;
        paraRxPHY.Fs = 4000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 224;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 480;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 672;
        paraRxPHY.LenN = 224;
        paraRxPHY.NumByte = 60;
        paraRxPHY.SegNum = 1*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum)*GWRxShrink;%1个包块对应2个window
        paraRxPHY.SymNumPN = 16;
    case 24
        paraRxPHY.Mfsk = 0;
        paraRxPHY.SymbolRate = 125/32;
        paraRxPHY.Fs = 4000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 4*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 224;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 480;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 672;
        paraRxPHY.LenN = 224;
        paraRxPHY.NumByte = 60;
        paraRxPHY.SegNum = 1*GWRXDataWindowNum*GWRxShrink;%2个包块对应2个window
        paraRxPHY.SymNumPN = 16;
    case 25
        paraRxPHY.Mfsk = 0;
        paraRxPHY.SymbolRate = 125/16;
        paraRxPHY.Fs = 4000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 4*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 224;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 480;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 672;
        paraRxPHY.LenN = 224;
        paraRxPHY.NumByte = 60;
        paraRxPHY.SegNum = 2*GWRXDataWindowNum*GWRxShrink;%4个包块对应2个window
        paraRxPHY.SymNumPN = 16;
    case 26
        paraRxPHY.Mfsk = 0;
        paraRxPHY.SymbolRate = 125/8;
        paraRxPHY.Fs = 4000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 4*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 224;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 480;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 672;
        paraRxPHY.LenN = 224;
        paraRxPHY.NumByte = 60;
        paraRxPHY.SegNum = 4*GWRXDataWindowNum*GWRxShrink;%8个包块对应2个window
        paraRxPHY.SymNumPN = 16;
    case 27
        paraRxPHY.Mfsk = 4;
        paraRxPHY.SymbolRate = 125/128/2;
        paraRxPHY.Fs = 16000* paraRxPHY.SymbolRate*(SysSimulationPara.GWRxSampleRate/125000);    % Sample rate (Hz)
        paraRxPHY.LenSym = 16*(SysSimulationPara.GWRxSampleRate/125000);                    % 符号长度，多少个采样点
        paraRxPHY.SymNumData = 52;         % 数据symbol个数
        paraRxPHY.InfoBitLen = 96;              %信息比特个数
        paraRxPHY.intrlvIndexLen = 108;
        paraRxPHY.LenN = 104;
        paraRxPHY.NumByte = 10;
        paraRxPHY.SegNum = 1*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum)*GWRxShrink;%1个包块对应2个window
        paraRxPHY.SymNumPN = 9;   
end

if  paraRxPHY.Mfsk == 4
    paraRxPHY.Fs2B = 16*(SysSimulationPara.GWRxSampleRate/125000);
    paraRxPHY.FsB = 125/ paraRxPHY.SymbolRate;
    switch( paraRxPHY.SymbolRate)
        case 125/128/2
            paraRxPHY.FilterLenB = 160*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 504*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
%             paraRxPHY.LenC1 = 3040*GWRxShrink*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum);
            paraRxPHY.DataStart = ceil((512)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值
        case 125/128
            paraRxPHY.FilterLenB = 320*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
%             paraRxPHY.LenC1 = 3040*GWRxShrink*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum);
            paraRxPHY.DataStart = ceil((1016+10)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值
        case 125/64
            paraRxPHY.FilterLenB = 320*2*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*2*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
%             paraRxPHY.LenC1 = 3040*1*GWRxShrink*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil((2040+10)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值
        case 125/32
            paraRxPHY.FilterLenB = 320*4*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*4*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
%             paraRxPHY.LenC1 = 3040*2*GWRxShrink*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil((4088+10)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值 需要根据时偏最大与最小仿真，得到最中心的开始位置
        case 125/16
            paraRxPHY.FilterLenB = 320*8*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*8*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
%             paraRxPHY.LenC1 = 3040*4*GWRxShrink*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil((8192+4)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值8192+4
    end
else
    paraRxPHY.Fs2B = 8*(SysSimulationPara.GWRxSampleRate/125000);
    paraRxPHY.FsB = 125/ paraRxPHY.SymbolRate;
    switch( paraRxPHY.SymbolRate)
        case 125/64
            paraRxPHY.FilterLenB = 128*2*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.LenC1 = 3040*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000)*floor(GWRXDataWindowNum/GWRXBasicDataWindowNum);
            paraRxPHY.DataStart = ceil(1024*GWRxShrink);%经验值1022
        case 125/32
            paraRxPHY.FilterLenB = 128*4*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*2*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.LenC1 = 3040*1*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000)*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil(2040*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%2040经验值
        case 125/16
            paraRxPHY.FilterLenB = 128*8*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*4*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.LenC1 = 3040*2*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000)*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil(4088*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值
        case 125/8
            paraRxPHY.FilterLenB = 128*16*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.SlideLenC1 = 1008*8*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000);
            paraRxPHY.LenC1 = 3040*4*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000)*GWRXDataWindowNum;
            paraRxPHY.DataStart = ceil((8192+8)*GWRxShrink*(SysSimulationPara.GWRxSampleRate/125000));%经验值
    end
end


iTmp=0:1:paraRxPHY.LenSym-1;
tmpP=1.5*iTmp/paraRxPHY.LenSym;
freq1 = exp(1i*2*pi*tmpP).';
tmpP=0.5*iTmp/paraRxPHY.LenSym;
freq2 = exp(1i*2*pi*tmpP).';
tmpP=-0.5*iTmp/paraRxPHY.LenSym;
freq3 = exp(1i*2*pi*tmpP).';
tmpP=-1.5*iTmp/paraRxPHY.LenSym;
freq4 = exp(1i*2*pi*tmpP).';

paraRxPHY.FskFreqs = [freq1 freq2 freq3 freq4];

paraRxPHY.LenC1 = ((GWRXPilotWindowNum+GWRXDataWindowNum)*8192-(GWRXPilotWindowNum+GWRXDataWindowNum-1)*128)/(paraRxPHY.FsB/paraRxPHY.Fs2B)*GWRxShrink;
SegLen1 = (paraRxPHY.LenC1 - paraRxPHY.DataStart)/paraRxPHY.SegNum;
paraRxPHY.SegLen = ceil(SegLen1);

if paraRxPHY.Mfsk == 4
    paraRxPHY.C2Adj = 48;
%     paraRxPHY.SegLen = 1968;
else
%     paraRxPHY.C2Adj = paraRxPHY.SegLen - (paraRxPHY.SymNumData+ paraRxPHY.SymNumPN+1)*8*(SysSimulationPara.GWRxSampleRate/125000);
    paraRxPHY.C2Adj = paraRxPHY.SegLen - (paraRxPHY.SymNumData+ paraRxPHY.SymNumPN+1)*8*(SysSimulationPara.GWRxSampleRate/125000);
%     paraRxPHY.C2Adj = paraRxPHY.SegLen - (paraRxPHY.SymNumData+ paraRxPHY.SymNumPN+1)*8;
end

% paraRxPHY.TOScope = floor(paraRxPHY.Fs*0.003);
paraRxPHY.TOScope = floor(paraRxPHY.Fs*(SysSimulationPara.GWRxSampleRate/125000)*0.003);
% paraRxPHY.TOScope = floor(paraRxPHY.Fs*0.0054);
paraRxPHY.ifftLen = (8192*GWRxShrink)/(paraRxPHY.FsB/paraRxPHY.Fs2B); 
paraRxPHY.OneBlockDataLen = 2 * paraRxPHY.ifftLen;
end