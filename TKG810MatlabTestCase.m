
function [UplinkLostPer,S_uplink,DownlinkLostPer,S_Downlink] = TKG810MatlabTestCase(CaseNum,SimFreamNum,SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara)

DownlinkLostPer = 1;
S_Downlink = 0;
%%
flodpath = ['./MatlabSaveData/case',num2str(CaseNum)];
if ~exist(flodpath,'dir')
   mkdir(flodpath);
end
folderPath0 = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum)];
if ~exist(folderPath0,'dir')
    mkdir(folderPath0);
end
folderPath1 = ['D:/data/K2301/K2301PHY_CaseList/LostRate/case',num2str(CaseNum)];
if ~exist(folderPath1,'dir')
    mkdir(folderPath1);
end
folderPath = ['D:/data/K2301/K2301PHY_CaseList/LostRate/case',num2str(CaseNum),'/log.txt'];
if ~exist(folderPath,'file')
    fid0 = fopen(folderPath,'w');
    if fid0 == -1
        error('creat log.txt fail');
    end
    fclose(fid0);
end
%% 
%  filename = sprintf('./Log/Case_%d.txt',CaseNum);
filename = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum),'/log.txt'];
% fid = fopen(filename,'a+');
fid = fopen(filename,'a+');
fprintf(fid,'date=%s\n',date);

NF = SimPara.NoiseFactor;
%sysReciprocal module
[GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactor,GWRXSNR_REG,GWTXSNR_REG] = GWAntennaRecipracal(SimPara,GWRXPara);
% [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactor] = GWAntennaRecipracalNew(SimPara,GWRXPara);
%%
DownLinkSimSwitch = SimPara.DownLinkSimSwitch;%>0 Open DownLink Sim ==0 close DownLink Sim
UpLinkSimSwitch = SimPara.UpLinkSimSwitch;%>0 Open UpLink Sim ==0 close UpLink Sim
%%
% for M=-10:1:2
AllGWRxPackageNum = 0;
AllGWTxPackageNum = 0;
AllTerminalTxPackageNum = 0;
AllTerminalRxPackageNum = 0;
BWDL = 2000;
CaseNum
TempSNR = [];
TempSNR1 = [];

GWRXLostPkg = zeros(1,TMTXPara.TxUserNum+50);
TMRXLostPkg = zeros(1,TMTXPara.TxUserNum+50);
PdBSum(1:2,1:SimFreamNum) = 0;
LostNum = 0;
for Iter = 1:SimFreamNum

    for Kter = 1:UpLinkSimSwitch
        s = RandStream('mt19937ar','Seed',Iter*UpLinkSimSwitch+Kter);
        RandStream.setGlobalStream(s);
        GWRxUserNum = 0;
        TerminalRxNum = 0;
        TerminalTxNum = TMTXPara.TxUserNum;
%         SimPara.GWRxSNR = M;
        %% SimParaInit
        [TMTxFreq,TMTxMode,TMTxPower,TxData,Fbits,Sbits,TxImagePower,TMRxMode,PathAttenuation] = SimCode_TerminalTxParaSet(SimPara,TMTXPara,GWRXPara,GWTXPara,Iter);
        
        %% TMTX Module
        parfor k=1:TMTXPara.TxUserNum
%         for k=1:TMTXPara.TxUserNum
            [TerminalTxSignal(k,:),BWUP(k)] = PHY2_1TerminalTxMainCode(SimPara,TMTXPara,GWRXPara,TMTxFreq(k),TMTxMode(k),TMTxPower(k),TxData(k,:),Fbits(k,:),Sbits(k,:),TxImagePower(k));
        end
        % [Pxif1, fif1] = pwelch(TerminalTxSignal(1,:),[], [], [],TMTXPara.SampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
        %% UplinkChannel Module
        [GW_Rx_MutiAntenna,H,SnrSamplingRate,TO] = UpLinkChannelModel(SimPara,TMTXPara,GWRXPara,GWAntennaRxPhaseNoReciprocal,TerminalTxSignal,BWUP(1),PathAttenuation,Iter);
        %% GWRX Module
        [GWRxUserInf,GWRxUserNum,LostNum0,TempUserID,ANoise] = GWPHYRx(CaseNum,GW_Rx_MutiAntenna,GWRXPara,Iter,SimPara.GWRxSNR);
%         [GWRxUserInf,GWRxUserNum,TempUserID,ANoise] = GWPHYRx_c(GW_Rx_MutiAntenna,GWRXPara,Iter);
        LostNum = LostNum + LostNum0;
        if GWRxUserNum >= 1
            GWRXLostPkg(TempUserID) = GWRXLostPkg(TempUserID) + 1;
        end
        %% GWRX result record
        if GWRxUserNum<TerminalTxNum || GWRxUserNum>TerminalTxNum
%             fprintf(fid,'PackageNum = %g,GWRxUserNum=%g\n',Iter,GWRxUserNum);
        end
        if GWRxUserNum>TerminalTxNum
            GWRxUserNum = TerminalTxNum;
        end
        if GWRxUserNum>0
            GWRxUserInfAll(Iter).T = GWRxUserInf;
            TempSNR(Iter) = GWRxUserInf(1).SNR;
            PdBSum(1,Iter) = PdBSum(1,Iter) + GWRxUserInf(1).PdBTmp(1,2);
            PdBSum(2,Iter) = PdBSum(2,Iter) + GWRxUserInf(1).PdBTmp(1,3);
        end
        AllGWRxPackageNum = AllGWRxPackageNum + GWRxUserNum;
        AllTerminalTxPackageNum = AllTerminalTxPackageNum + TerminalTxNum;
        
        DownLinkSimSwitch = 0;
        
        if DownLinkSimSwitch>0
            if GWRxUserNum==0
                continue;
            end
            [H_DownLink,PathAttenuation_1,GWTXMode,GWTXPower,GWTXAH,GWTxData,Fbits1,Sbits1,GWTXFreq,TMRXFreq,GWRxPilotPower] = ...
                SimCode_UserAHMatch(DownLinkSimSwitch,H,GWAntennaRxPhaseNoReciprocal,PathAttenuation,TMTxPower,TMTxFreq,TMRxMode,GWRxUserNum,TempUserID,GWRxUserInf,TMTXPara);
            
            %% GWTX Module
            %gwtx_dump(GWTXAH,GWRxPilotPower,GWTXMode,GWTXFreq,GWTxData,Fbits1,Sbits1,GWRxUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor,Iter, 10);
            %[GWTXAH,GWRxPilotPower,GWTXFreq,ANoise,CalibrateFactor, GWRxUserNum, GWTXMode, GWTxData] = gwtx_load(4);
            [GW_Tx_MutiAntennaSignal,ModeParaDown,PowerTxdBm] = PHYGWTx(GWTXAH,GWRxPilotPower,GWTXMode,GWTXFreq,GWTxData,Fbits1,Sbits1,GWRxUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor,Iter);
            
            %% DownLinkChannel Module

            [TerminalRxSignal,TO_1] = DownLinkChannelModel(SimPara,GWAntennaTxPhaseNoReciprocal,GWTXPara,GW_Tx_MutiAntennaSignal,H_DownLink,GWRxUserNum,ModeParaDown,PathAttenuation_1,Iter);
%             [Pxif1, fif1] = pwelch(TerminalRxSignal(1,:),[], [], [],TMRXPara.TMRXSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
            %% TerminalRx Module
            [TerminalRxInf,TerminalRxNum,TempUserID] = PHY2_1TerminalRx(TerminalRxSignal,TMRXPara,TMRXFreq,GWTXMode,GWRxUserNum);

            %% TerminalRX result record
            TempUserID_o = sort(TempUserID,'ascend');
            TempUserID_o(find(TempUserID_o>TerminalTxNum)) = TerminalTxNum;
            if TerminalRxNum >= 1
                BWDL = ModeParaDown(1).BW;
                TMRXLostPkg(TempUserID_o) = TMRXLostPkg(TempUserID_o) + 1;
            end
            if TerminalRxNum < GWRxUserNum
                fprintf(fid,'PackageNum = %g,GWTxUserNum=%g,TerminalRxNum=%g\n',Iter,GWRxUserNum,TerminalRxNum);
            end
            if TerminalRxNum>0
                TerminalRxUserInfAll(Iter).T = TerminalRxInf;
                TempSNR1(Iter) = mean(TerminalRxInf(1).SnrFsk_dqpsk);
            end
            AllTerminalRxPackageNum = AllTerminalRxPackageNum + TerminalRxNum;
            AllGWTxPackageNum = AllGWRxPackageNum;
        end
    end
    Iter
end
%% Uplink
UplinkLostPer = (AllTerminalTxPackageNum-AllGWRxPackageNum)/AllTerminalTxPackageNum;
TestLostPer = LostNum/(SimFreamNum*GWRXPara.GWRxUserNum);
S_uplink = -174+NF+SimPara.GWRxSNR+10*log10(BWUP(1)*125/128);
GWRXLostRate = (SimFreamNum-GWRXLostPkg)./SimFreamNum;
Name = sprintf('./Log/GWRXLostRate_%d.mat',CaseNum);
save(Name,'GWRXLostRate');
fprintf(fid,'\n C Matlab Case = %d,S_Up = %g,AllGWRxPackageNum = %g,AllTerminalTxPackageNum = %.3f,UplinkLostPer=%.3f,MeanSNR = %.2f,MeanPdB1 = %.2f,MeanPdB2 = %.2f,VarPdB1 = %.2f,VarPdB2 = %.2f,TestLostPer = %.5f\n',...
    CaseNum,S_uplink,AllGWRxPackageNum,AllTerminalTxPackageNum,UplinkLostPer,mean(TempSNR),mean(PdBSum(1,:)),mean(PdBSum(2,:)),var(PdBSum(1,:)),var(PdBSum(2,:)),TestLostPer);
%% Downlink
if DownLinkSimSwitch>0
    DownlinkLostPer = (AllGWTxPackageNum-AllTerminalRxPackageNum)/AllGWTxPackageNum;
    S_Downlink = -174+NF+SimPara.TMRxSNR+10*log10(BWDL*125/128);
    TMRXLostRate = (GWRXLostPkg-TMRXLostPkg)./GWRXLostPkg;
    Name1 = sprintf('./Log/TMRXLostRate_%d.mat',CaseNum);
    save(Name1,'TMRXLostRate');
    fprintf(fid,'\n C Matlab Case = %d,S_Down = %g,AllTerminalRxPackageNum = %g,AllGWTxPackageNum = %.3f,DownlinkLostPer=%.3f,MeanSNR = %.2f\n',...
        CaseNum,S_Downlink,AllTerminalRxPackageNum,AllGWTxPackageNum,DownlinkLostPer,mean(TempSNR1));
end
mean(PdBSum(1,:))
% end
%%
fclose(fid);
end