function [TxSignal,ModeParaUp] = PHY2_1GWTxSignalPrepareCode(TxMode,GWTXPara,GWTxData,Fbits,Sbits)

%% 初始化参数计算
ModeParaUp = [];
[ModeParaUp] = GetmodelparaGWTxPHY2_1(TxMode,GWTXPara);

%% 发射机
MACTxByte = ModeParaUp.Len_TxByte - 2;
PHYTxByte = ModeParaUp.Len_TxByte;
DataTxByte = GWTxData(1,:);
if ModeParaUp.crc14_en == 1
    for j=1:ModeParaUp.TxBlockNum
        for i=1:MACTxByte
            temp = bitget(DataTxByte(1,i+(j-1)*MACTxByte),8:-1:1,'uint8');
            infoBits(1,(8*i-7)+(j-1)*PHYTxByte*8:(8*i)+(j-1)*PHYTxByte*8) = temp;
        end
        infoBits(1,j*PHYTxByte*8-15) = Sbits(j);
        infoBits(1,j*PHYTxByte*8-14) = Fbits(j);
        infoBits(1,j*PHYTxByte*8-13:j*PHYTxByte*8) = zeros(1,14);
    end
else
    for j=1:ModeParaUp.TxBlockNum
        for i=1:PHYTxByte
            temp = bitget(DataTxByte(1,i+(j-1)*PHYTxByte),8:-1:1,'uint8');
            infoBits(1,(8*i-7)+(j-1)*PHYTxByte*8:(8*i)+(j-1)*PHYTxByte*8) = temp;
        end
    end
end
infoBits = infoBits.';
InfoBitsOneSlot = infoBits;
%% 模块1： DPFSK信号调制模块   输入：输入比特（整个slot），包块比特长度， 编码器选择， 码长， 包数， fsk模式， 扩频模式    输出：编码比特（整个slot）  for循环包块
[SigTxSym] = PHY2_1Tx(InfoBitsOneSlot,ModeParaUp);
m = log2(GWTXPara.GWTxSampleRate/ModeParaUp.FsTx);
UpSampleSigTxSym0 = upsampling_new(SigTxSym,m);

DataPreLen = (ModeParaUp.PilotLen + ModeParaUp.AGCLen)*GWTXPara.GWTxSampleRate;
if(TxMode == 11)
    DataPreLen = 40;
elseif(TxMode == 12)
    DataPreLen = 40;
elseif(TxMode == 13)
    DataPreLen = 40;
elseif(TxMode == 14)
    DataPreLen = 48;
elseif(TxMode == 15)
    DataPreLen = 96;
elseif(TxMode == 16)
    DataPreLen = 160;
elseif(TxMode == 17)
    DataPreLen = 192;
elseif(TxMode == 18)
    DataPreLen = 256;
elseif(TxMode == 19)
    DataPreLen = 256;
elseif(TxMode == 20)
    DataPreLen = 256;
end
DataPreLen = DataPreLen * 2^m;
AddToneLen = GWTXPara.GWTxToneEn * GWTXPara.GWTxSampleRate;
UpSampleSigTxSym = [-1*ones(1,DataPreLen) UpSampleSigTxSym0 -1*ones(1,AddToneLen) ];
sigTxUpsampleOut = exp(1*pi*1j*UpSampleSigTxSym);
% TxSignal = sigTxUpsampleOut(1:end-2^m+1);
TxSignal = sigTxUpsampleOut;%(1:end-2^m+1);
end