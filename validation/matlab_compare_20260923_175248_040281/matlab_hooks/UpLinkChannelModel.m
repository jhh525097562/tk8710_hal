function [GW_Rx_MutiAntenna_o,AHOut,SnrSamplingRate,TO] = UpLinkChannelModel(SimPara,TMTXPara,GWRXPara,GWAntennaRxPhaseNoReciprocal,TerminalTxSignal,BW,PathAttenuation,Iter)


M = GWRXPara.AntennaNum;
s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);

SnrSamplingRate = SimPara.GWRxSNR-10*log10(TMTXPara.SampleRate/(BW*125/128))-pow2db(50);
NoiseFlag = SimPara.GWRxNoiseFlag;
TxUserNum = TMTXPara.TxUserNum;
noiseVar = 1/10^(SnrSamplingRate/10);

parfor (it=1:TxUserNum,0)
% for it=1:TxUserNum
    [Rx(:,it),TO(it)] = OneUserChannelModel(TerminalTxSignal(it,:),SimPara,TMTXPara.SampleRate,SnrSamplingRate,PathAttenuation(it));
end

% AHOut = SysAHGen(SysSimulationPara,TxUserNum,1);
% AHOut = SysAHGen(SysSimulationPara,TxUserNum,PrintCaseNum);
AHOut = SysAHGen(SimPara,GWRXPara,TxUserNum,Iter);
if SimPara.SingleToneInterferenceFlag==0%No SingleTone Interference
    if SimPara.Periodic_InterferenceFlag==0%No Periodic Interference
        Rxtemp = Rx.';
    else
        PeriodicInterSignal = PeriodicInterference_SignalGen(SimPara,TMTXPara,GWRXPara,length(Rx),40,Iter);
        Rx(:,TxUserNum+1) = PeriodicInterSignal;
        AHOut(:,TxUserNum+1) = SysAHGen(SimPara,GWRXPara,1,1);
        Rxtemp = Rx.';
    end
elseif SimPara.SingleToneInterferenceFlag==1%add SingleTone Interference
    InterferToneSignal = InterferenceToneGen(SimPara,GWRXPara,TMTXPara,length(Rx),10);
    Rx(:,TxUserNum+1) = InterferToneSignal;
    AHOut(:,TxUserNum+1) = SysAHGen(SimPara,GWRXPara,1,1);
    if SimPara.Periodic_InterferenceFlag==0%No Periodic Interference
        Rxtemp = Rx.';
    else%have Periodic Interference
        PeriodicInterSignal = PeriodicInterference_SignalGen(SimPara,TMTXPara,GWRXPara,length(Rx),40,Iter);
        Rx(:,TxUserNum+2) = PeriodicInterSignal;
        AHOut(:,TxUserNum+2) = SysAHGen(SimPara,GWRXPara,1,2);
        Rxtemp = Rx.';
    end
elseif SimPara.SingleToneInterferenceFlag==2%add WideBand Interference
    blockBW = SimPara.SingleToneInterferenceNum;%带宽
    PowerDiff = SimPara.SingleToneInterferencePower;%比用户信号低XdB
    [InterferSignal] = InterferenceWideBandSignalGen(TMTXPara.SampleRate,length(Rx),blockBW,PowerDiff,Iter);
    Rx(:,TxUserNum+1) = InterferSignal;
    AHOut(:,TxUserNum+1) = SysAHGen(SimPara,GWRXPara,1,1);
    if SimPara.Periodic_InterferenceFlag==0%No Periodic Interference
        Rxtemp = Rx.';
    else%have Periodic Interference
        PeriodicInterSignal = PeriodicInterference_SignalGen(SimPara,TMTXPara,GWRXPara,length(Rx),40,Iter);
        Rx(:,TxUserNum+2) = PeriodicInterSignal;
        AHOut(:,TxUserNum+2) = SysAHGen(SimPara,GWRXPara,1,2);
        Rxtemp = Rx.';
    end
end
    
if NoiseFlag==0%not add Noise
    RxOut = AHOut*Rxtemp;
else%add Noise
    AntennaNoisePower = db2pow(SimPara.GWRxAnoisePower(1:M)).';
    Noise = (randn(M,length(Rxtemp)) + (1j)*randn(M,length(Rxtemp))) * sqrt(noiseVar/2).*sqrt(AntennaNoisePower);
    RxOut = AHOut*Rxtemp + Noise;
end

% for k=1:8
%     PathDelays = [0 0.1e-6 0.15e-6 0.3e-6 0.5e-6];  % Path delays (s)
%     PathPower =  [-4  -3  0  -2.6  -3.0];   % Path power (dB)
%     rayChan = comm.RayleighChannel('SampleRate',1000000,'PathDelays',PathDelays,'AveragePathGains',PathPower, 'NormalizePathGains',true, ...
%         'MaximumDopplerShift',0,'FadingTechnique','Sum of sinusoids','RandomStream','mt19937ar with seed','Seed',randi(10000),'PathGainsOutputPort',true);
%     [RxTmp, HTmp] = step(rayChan,Rxtemp(1,:).');
%     H1(:,k) = sum(HTmp,2);
% end
% RxOut = RxOut.*H1.';
%RFDelaySim
if SimPara.GWRXRFDelayFlag==1
    if SimPara.AntennaNum>=2
        DelayRF = randi(4);
        DelayTime = ceil(randi(length(RxOut))/50);
        RxOut(DelayRF*2-1:DelayRF*2,:) = [zeros(2,DelayTime) RxOut(DelayRF*2-1:DelayRF*2,1:length(RxOut)-DelayTime)];
    end
end

% GWRXSignal NoReciprocal
GW_Rx_MutiAntenna = RxOut.*GWAntennaRxPhaseNoReciprocal;
% Set GWRXGAIN
GWRXGain = db2pow(SimPara.GWRxAntennaGain).';
GW_Rx_MutiAntenna_o = GW_Rx_MutiAntenna.*sqrt(GWRXGain(1:M));


end

