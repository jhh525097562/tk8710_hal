
close all;
clc;
clear all;
addpath(genpath('1.0PHY2.0发射机代码(终端)'));
addpath(genpath('1.1PHY2.0接收机代码(网关)'));
addpath(genpath('1.2PHY2.0发射机代码(网关)'));
addpath(genpath('1.3PHY2.0接收机代码(终端)'));
addpath(genpath('Log'));
%%
%TestCaseParaIn
TestCaseList = xlsread('TestCaseList.xlsx');
CaseNum = length(TestCaseList(:,1));
%% 信道模型参数
Tag  = 'CFO';  % 1: CFO    2:Doppler    3:  CFODoppler
SimFreamNum = 1;
fid = fopen('仿真结果记录','a+');
for iS = 17:18
    %SysParaIn
    SysParameter = TestCaseList(iS,end);
    file_name=[num2str(SysParameter) '.xlsx'];
    SysParameters = xlsread(file_name);
    [SysPara,GWRxPara,GWTxPara] = SysParametersSet(SysParameters);
    
    [SysSimulationPara,TerminalTxRxPara,TerminalTxNum] = TestCaseListParaSet(TestCaseList(iS,:));
    NF = SysSimulationPara.NoiseFactor;
    %互易性校准
    if SysSimulationPara.AntennaNum==1
        GWAntennaRxPhaseNoReciprocal(1:SysSimulationPara.AntennaNum) = ones(1,SysSimulationPara.AntennaNum);
    else
        [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactor] = GWAntennaRecipracal(SysSimulationPara);        
    end


    AllGWRxPackageNum = 0;
    AllTerminalTxPackageNum = 0;
    AllTerminalRxPackageNum = 0;
    for Iter = 1:SimFreamNum
        %test 
        s = RandStream('mt19937ar','Seed',Iter);
        RandStream.setGlobalStream(s);
        GWRxUserNum = 0;
        TerminalRxNum = 0;
        %% 终端发射信号准备
        [TxFreq,TxMode,TxDataPower,TxData,TxImagePower] = TerminalTxParaSet(TerminalTxNum,TerminalTxRxPara,SysPara,Iter);
        TerminalTxSignal = PHY2_0TerminalTxMainCode(TerminalTxNum,TxFreq,TxMode,TxDataPower,TxImagePower,TxData,GWRxPara,TerminalTxRxPara,SysSimulationPara);
        % [Pxif1, fif1] = pwelch(TerminalTxSignal(1,:),[], [], [],SysPara.TerminalTxSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
        %% 上行信道模型
        [GW_Rx_MutiAntenna,H,SnrSamplingRate,TO] = UpLinkChannelModel(Tag,TerminalTxNum,TerminalTxSignal,TerminalTxRxPara,SysSimulationPara,Iter);
        GWSigDownSampleRx = Downsampling(GW_Rx_MutiAntenna,SysSimulationPara);
        %% 网关接收机
        % 上行信道不互易性
        GWSigDownSampleRx = GWSigDownSampleRx.*GWAntennaRxPhaseNoReciprocal;
        [GWRxUserInf,GWRxUserNum] = PHY2GWRx(GWSigDownSampleRx,SysSimulationPara,SysPara,GWRxPara,Iter);
%         %% 网关发射机
%         % 下行发送的SNR设定
%         DownlinkSnrAt1KHz = [6.8 - 9 + 30]; %
%         [GW_Tx_MutiAntennaSignal,GWTxFreq,GWTxMode,SnrAt1KHz,ModeParaDown] = PHY2GWTx(DownlinkSnrAt1KHz,GWRxUserInf,GWRxUserNum,ED_Parameters,CalibrateFactor,SysPara,GWRxPara,GWTxPara);
%         % 下行信道不互易性
%         GW_Tx_MutiAntennaSignal = GW_Tx_MutiAntennaSignal.*GWAntennaTxPhaseNoReciprocal;
%         %% 下行信道模型
%         TerminalRxSignal0 = H.'*GW_Tx_MutiAntennaSignal;
%         [TerminalRxSignal, TO_1] = DownLinkChannelModel(CFOStruct,DopplerStruct,TerminalRxSignal0,Tag,0,ImpulseNoiseFlag,0,0,SysPara.AntennaNum,GWRxUserNum,ModeParaDown,SnrAt1KHz,UpDowmSampleFlag);
%         % [Pxif1, fif1] = pwelch(TerminalRxSignal(2,:),[], [], [],SysPara.TerminalTxSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
%         %% 终端解调译码模块
%         [TerminalRxInf,TerminalRxSignal1,TerminalRxNum] = PHY2TerminalRx(TerminalRxSignal,SysPara.TerminalRxSampleRate,GWTxFreq,ModeParaDown,ED_Parameters,GWTxMode,GWRxUserNum);
        %% 数据分析与保存
        if GWRxUserNum<TerminalTxNum%网关上行接收丢包
            fprintf(fid,'PackageNum = %g,GWRxUserNum=%g\n',Iter,GWRxUserNum);
        end
        %网关上行接收结果保存
        if GWRxUserNum>0
            GWRxUserInfAll(Iter).T = GWRxUserInf;
            TempSNR(Iter) = GWRxUserInf(1).SNR;
        end
        %网关上行接收丢包统计
        AllGWRxPackageNum = AllGWRxPackageNum + GWRxUserNum;
        AllTerminalTxPackageNum = Iter*TerminalTxNum;
%         %网关下行发射结果保存
%         if TerminalRxNum < GWRxUserNum%网关下行发送丢包
%             fprintf(fid,'PackageNum = %g,GWTxUserNum=%g,TerminalRxNum=%g\n',Iter,GWRxUserNum,TerminalRxNum);
%         end
%         if TerminalRxNum>0
%             TerminalRxUserInfAll(Iter).T = TerminalRxInf;
%         end
%         %网关下行发射丢包统计
%         AllTerminalRxPackageNum = AllTerminalRxPackageNum + TerminalRxNum;
%         AllGWTxPackageNum = AllGWRxPackageNum;
        Iter
    end
% 上行仿真结果保存
UplinkLostPer(iS) = (AllTerminalTxPackageNum-AllGWRxPackageNum)/AllTerminalTxPackageNum
S_uplink(iS) = -174+NF+SysSimulationPara.GWRxSNR+10*log10(4*125/128*1000)
fprintf(fid,'\n\n\nCase = %d,SysConfigName = %d,S_Up = %g,AllGWRxPackageNum = %g,AllTerminalTxPackageNum = %.2f,UplinkLostPer=%.2f,MeanSNR = %.2f\n',...
                  iS,SysParameter,S_uplink(iS),AllGWRxPackageNum,AllTerminalTxPackageNum,UplinkLostPer(iS),mean(TempSNR));
% 下行仿真结果保存
% save TerminalRxUserInfAll TerminalRxUserInfAll;
% DownlinkLostPer(iS) = (AllGWTxPackageNum-AllTerminalRxPackageNum)/AllGWTxPackageNum
% S_Downlink(iS) = -174+NF+30+SnrAt1KHz(1)-10*log10(1000/ModeParaDown(1).SymbolRate)
% fprintf(fid,'S_Dowm = %g,iS = %g,GWTxMode = %g,2f,AllTerminalRxPackageNum = %g,AllGWTxPackageNum = %.2f,DownlinkLostPer=%.2f\n\n\n',...
%                   S_Downlink(iS),iS,GWTxMode(1),AllTerminalRxPackageNum,AllGWTxPackageNum,DownlinkLostPer(iS));
iS
end
fclose(fid);