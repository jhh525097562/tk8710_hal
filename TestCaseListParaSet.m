function [SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara] = TestCaseListParaSet(TestCaseList)
AntennaNum = TestCaseList(1);
RFSampleRate = TestCaseList(2);
%% SIMPara
SimPara.AntennaNum = AntennaNum;
SimPara.SampleRate = RFSampleRate;

SimPara.UpLinkSimSwitch = TestCaseList(3);%==1 Open DownLink Sim ==0 close DownLink Sim
SimPara.DownLinkSimSwitch = TestCaseList(4);%==1 Open DownLink Sim ==0 close DownLink Sim
SimPara.RecipracalCaliFlag = TestCaseList(5);%==0 close Rx Tx NoRecipracal ==1 open
SimPara.CalibrateFactorDiff = TestCaseList(6);%

SimPara.GWRxSNR = TestCaseList(7);
SimPara.TMRxSNR = TestCaseList(8);

SimPara.NoiseFactor = TestCaseList(9);
SimPara.AHFlag = TestCaseList(10);

SimPara.GWRxShrink = TestCaseList(11);
SimPara.GWRXRFDelayFlag = TestCaseList(12);

SimPara.GWRxNoiseFlag = TestCaseList(13);
SimPara.GWRxAnoisePower = TestCaseList(14:21);
SimPara.GWRxAntennaGain = TestCaseList(22:29);
SimPara.AntennaWorkStatus = TestCaseList(30);

SimPara.GWTxPowerFlag = TestCaseList(31);%
SimPara.GWTxAHSelectFlag = TestCaseList(32);%

SimPara.TXFreqType = TestCaseList(33);
SimPara.TxFreqInterval = TestCaseList(34);
SimPara.CFOFreqFlag = TestCaseList(35);%CFO Flag ==0 Close CFO Sim ==1 Open CFO Sim
SimPara.CFOFreqSet = TestCaseList(36);%CFO Config
SimPara.DriftPerSecFlag = TestCaseList(37);%DriftPerSec Flag ==0 Close Drift Sim ==1 Open Drift Sim
SimPara.DriftPerSecSet = TestCaseList(38);%Drift Config
SimPara.TimingOffsetFlag = TestCaseList(39);%TimeOffset Flag ==0 Close TimeOffset Sim ==1 Open TimeOffset Sim
SimPara.TimingOffsetSet = TestCaseList(40);%TimeOffset Config
SimPara.TxPowerDiffSet = TestCaseList(41);%
SimPara.Tx_Image_InterferenceFlag = TestCaseList(42);
if SimPara.Tx_Image_InterferenceFlag==0
    SimPara.Tx_Image_InterferencePowerDiffSet = -100;%
else
    SimPara.Tx_Image_InterferencePowerDiffSet = TestCaseList(43);%
end
SimPara.Tx_ADCFlag = TestCaseList(44);%==0 
SimPara.Tx_ADCPowerDiffSet = TestCaseList(45);%
SimPara.Tx_AVGPowerDiffSet = TestCaseList(46);%  
SimPara.Tx_AVGPowerDiffUserNum = TestCaseList(47);%   
SimPara.DopplerFreq = TestCaseList(48);

SimPara.Periodic_InterferenceFlag = TestCaseList(49);
SimPara.Periodic = TestCaseList(50);
SimPara.Periodic_InterferencePower = TestCaseList(51);
SimPara.SingleToneInterferenceFlag = TestCaseList(52);
SimPara.SingleToneInterferenceNum = TestCaseList(53);
SimPara.SingleToneInterferencePower = TestCaseList(54);
X1=54;

%% TMTXPara
TMTXPara.SampleRate = RFSampleRate;
TMTXPara.TMTxBlockNum = TestCaseList(X1+1);
TMTXPara.TMTxByteLen = TestCaseList(X1+2);
TMTXPara.TMTxSrcFactor = TestCaseList(X1+3);
TMTXPara.TMTxMode = TestCaseList(X1+4);
TMTXPara.TMTxCRCEn = TestCaseList(X1+5);
TMTXPara.TxUserNum = TestCaseList(X1+6);
TMTXPara.FreqAllocationFlag = TestCaseList(X1+7);
TMTXPara.TMAllocationTxMode = TestCaseList(X1+8);%
TMTXPara.AllcationUserNum = TestCaseList(X1+9);
TMTXPara.AllocationTXFreqSet = [];
TMTXPara.TMAllocationTxMode = [];
X2 = X1 + 9;
% Allocation Freq Load
if TMTXPara.FreqAllocationFlag==1
    SysParameter = TestCaseList(end-2);
    file_name = [num2str(SysParameter) '.xlsx'];
    AllocationFreq = xlsread(file_name);
    for i=1:TMTXPara.AllcationUserNum
        TMTXPara.TMAllocationTxMode(i) = AllocationFreq(1,i);%
        TMTXPara.AllocationTXFreqSet(i) = AllocationFreq(2,i);%
        GWAllocationTXMode(i) = AllocationFreq(3,i);%
    end
end
TMTXPara.TxFreq = 0;
%% GWRXPara
GWRXPara.AntennaNum = AntennaNum;
GWRXPara.RFSampleRate = RFSampleRate;
GWRXPara.GWRxSampleRate = TestCaseList(X2+1);
GWRXPara.SRCFactor = TMTXPara.TMTxSrcFactor;
GWRXPara.GWRxBlockNum = TMTXPara.TMTxBlockNum;
GWRXPara.GWRxByteLen = TMTXPara.TMTxByteLen;
GWRXPara.GWRxStopS = TestCaseList(X2+2);
GWRXPara.GWRxStopCRC = TestCaseList(X2+3);
GWRXPara.GWRxFFTLen = TestCaseList(X2+4);
GWRXPara.GWRXFilterLen = TestCaseList(X2+5);
GWRXPara.GWRXIFFTLen = TestCaseList(X2+6);
GWRXPara.GWRxDataWindowNum = TestCaseList(X2+7);
GWRXPara.RxMode = TMTXPara.TMTxMode;
GWRXPara.GWRxUserNum = TestCaseList(X2+8);
GWRXPara.GWRXBFMode = TestCaseList(X2+9);
GWRXPara.GWRX_BFSelectFlag = TestCaseList(X2+10);
GWRXPara.GWRX_BFSelectThe = TestCaseList(X2+11);
GWRXPara.GWRXBFSelectAlpha = TestCaseList(X2+12);
GWRXPara.GWRX_ANoiseUpdataPer = TestCaseList(X2+13);
GWRXPara.GWRX_ANoiseThe1 = TestCaseList(X2+14);
GWRXPara.GWRX_ANoiseThe2 = TestCaseList(X2+15);
GWRXPara.GWRxP1_The = TestCaseList(X2+16);
GWRXPara.GWRX_P1_Para1 = TestCaseList(X2+17);
GWRXPara.GWRX_P1_Para2 = TestCaseList(X2+18);
GWRXPara.GWRxP2_The = TestCaseList(X2+19);
GWRXPara.GWRxP3_The = TestCaseList(X2+20);
GWRXPara.GWRxP6_The = TestCaseList(X2+21);
GWRXPara.FreqAllocationFlag = TMTXPara.FreqAllocationFlag;
GWRXPara.AllocationRXModeSet = TMTXPara.TMAllocationTxMode;%
GWRXPara.AllcationUserNum = TMTXPara.AllcationUserNum;
GWRXPara.AllocationRXFreqSet = TMTXPara.AllocationTXFreqSet;%
GWRXPara.AppointAHFlag = 0;
GWRXPara.AppointAHUserNum = 0;
GWRXPara.AppointAH = 0;
GWRXPara.AppointTChID = 0;
GWRXPara.AntennaWorkStatus = SimPara.AntennaWorkStatus;
GWRXPara.GWRxShrink = SimPara.GWRxShrink;
GWRXPara.GWRxCRCEn = TMTXPara.TMTxCRCEn;
X3 = X2 + 21;
GWRXPara.AntennaNum = 8;
%% GWTXPara
GWTXPara.AntennaNum = GWRXPara.AntennaNum;
GWTXPara.RFSampleRate = RFSampleRate;
GWTXPara.GWTxSampleRate = GWRXPara.GWRxSampleRate;
GWTXPara.GWTxMode = TestCaseList(X3+1);
GWTXPara.GWTxBlockNum = TestCaseList(X3+2);
GWTXPara.GWTxByteLen = TestCaseList(X3+3);
GWTXPara.GWTxToneEn = TestCaseList(X3+4);
GWTXPara.GWTxFreq = 0;
GWTXPara.GWTxCRCEn = TMTXPara.TMTxCRCEn;
if TMTXPara.FreqAllocationFlag==1
    GWTXPara.AllocationTXModeSet = GWAllocationTXMode;
end
GWTXPara.GWTxAHSelectFlag = SimPara.GWTxAHSelectFlag;
GWTXPara.RecipracalCaliFlag = SimPara.RecipracalCaliFlag;
GWTXPara.GWTxPowerFlag = SimPara.GWTxPowerFlag;
GWTXPara.AntennaWorkStatus = SimPara.AntennaWorkStatus;
%% TMRXPara
TMRXPara.TMRXSampleRate = RFSampleRate;
TMRXPara.RxMode = GWTXPara.GWTxMode;
TMRXPara.TMRxBlockNum = GWTXPara.GWTxBlockNum;
TMRXPara.TMRxByteLen = GWTXPara.GWTxByteLen;
TMRXPara.TMRXFreq = 0;
TMRXPara.TMRxCRCEn = TMTXPara.TMTxCRCEn;
end
