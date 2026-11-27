function [SysPara,GWRxPara,GWTxPara] = SysParametersSet(SysParameters,SysSimulationPara)
%% SysPara Set
SysPara.ModeNum = SysParameters(1,1);%SysPara.GWModeNum
SysPara.GWRxMode = SysParameters(2,1:SysPara.ModeNum);%
SysPara.GWRxModeBW = SysParameters(3,1:SysPara.ModeNum);%
SysPara.GWTxMode = SysParameters(4,1:SysPara.ModeNum);%
SysPara.GWTxBW = SysParameters(5,1:SysPara.ModeNum);%
SysPara.PolarCCSet = SysParameters(6,1);%0 CRR 1 polar
X1=length(unique(fieldnames(SysPara)))+1;
%% GWRxPara Set
GWRxPara.Shrink = SysParameters(X1+1,1);
GWRxPara.RxFFTlen = SysParameters(X1+2,1);
GWRxPara.RXPilotWindowNum = SysParameters(X1+3,1);
GWRxPara.RxBasicDataWindowNum = SysParameters(X1+4,1);
GWRxPara.CalculateSNR = SysParameters(X1+5,1);
GWRxPara.ANoisePara = SysParameters(X1+6,1);%
GWRxPara.ANoiseThe = SysParameters(X1+7,1);%
GWRxPara.ADC = SysParameters(X1+8,1:8).';%
GWRxPara.ANoise = SysParameters(X1+9,1:8).';%
X2=X1+length(unique(fieldnames(GWRxPara)))+1;
%% GWTxPara Set
GWTxPara.TxCaliPose = SysParameters(X2+1,1);%
GWTxPara.TxPowerAllocation = SysParameters(X2+2,1);%
X3=X2+length(unique(fieldnames(GWTxPara)))+1;
%% SysFreqAllocation
SysPara.AllocationRxModeSet = SysParameters(X3+1,1:SysSimulationPara.FreqAllocationNum);%
SysPara.AllocationFreqSet = SysParameters(X3+2,1:SysSimulationPara.FreqAllocationNum);%
SysPara.AllocationTxModeSet = SysParameters(X3+3,1:SysSimulationPara.FreqAllocationNum);%
end