function [GW_Tx_MutiAntennaSignal_o,ModeParaDown,PowerTxdBm,GW_Tx_MutiAntennaSignal,BeamformerMatrix,GWTxSignal] = PHYGWTx(GWRXAH,GWRxPilotPower,GWTXMode,GWTXFreq,GWTxData,Fbits,Sbits,DownlinkUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor,Iter)

[BeamformerMatrix,PowerTxdBm] = GW_Tx_Beamforming(GWRXAH,GWRxPilotPower,GWTXMode,GWTXFreq,DownlinkUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor);

s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);

PolarCCflag = 0;
if DownlinkUserNum>0
%     GWTxSignal(1:DownlinkUserNum,1:300000) = 0;
else
    
end

%% 检查天线工作状态，天线工作状态为0，则将输入数据置0
M = GWTXPara.AntennaNum; % 天线数
K = dec2bin(GWTXPara.AntennaWorkStatus,8);
for i=1:M
   if K(M-i+1)==dec2bin(1)
       
   else
       BeamformerMatrix(:,i) = 0;
   end
end

parfor ii=1:DownlinkUserNum
% for ii=1:DownlinkUserNum
    %% 下行每个用户发送的下行原始信号
    [SigTx,ModeParaDown(ii)] = PHY2_1GWTxSignalPrepareCode(GWTXMode(ii),GWTXPara,GWTxData(ii,:),Fbits,Sbits);        

    %% 变频到用户发送频率上
    tmpO = 0:1:length(SigTx)-1;
%     GWTxSignal(ii,1:length(SigTx)) = SigTx.*exp(1i*2*pi*tmpO*GWTXFreq(ii)/GWTXPara.GWTxSampleRate);
    GWTxSignal(ii,:) = SigTx.*exp(1i*2*pi*tmpO*round(GWTXFreq(ii))/GWTXPara.GWTxSampleRate);
end
% figure;plot(real(GWTxSignal(1,:)));%

%% 下行发送矩阵相乘模块
% Matrix_MultLen = 4096;
% Matrix_MultLen = 1024;
% Matrix_MultCount = ceil(length(GWTxSignal)/Matrix_MultLen);
% GWTxSignal = [GWTxSignal,zeros(DownlinkUserNum,Matrix_MultCount*Matrix_MultLen-length(GWTxSignal))];
% GW_Tx_MutiAntennaSignal = GW_Tx_Matrix_Mult_Module(GWTxSignal,BeamformerMatrix,Matrix_MultLen,Matrix_MultCount);
GW_Tx_MutiAntennaSignal = BeamformerMatrix.' * GWTxSignal;


GW_Tx_MutiAntennaSignal_o = UpsamplePHY2201(GW_Tx_MutiAntennaSignal,GWTXPara.RFSampleRate,GWTXPara.GWTxSampleRate);

end