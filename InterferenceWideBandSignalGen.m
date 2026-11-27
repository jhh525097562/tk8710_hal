function [InterferSignal] = InterferenceWideBandSignalGen(sampleRate,SignalLen,blockBW,PowerDiff,Iter)

    s = RandStream('mt19937ar','Seed',Iter);
    RandStream.setGlobalStream(s);
%     frePointPos = randi([-SysSimulationPara.GWRxSampleRate/2+blockBW SysSimulationPara.GWRxSampleRate/2-blockBW],1);
%     frePointPos = 0;
%     n4MHz=SignalLen;      %采样点个数
%     T4MHz=n4MHz/sampleRate;                 %时宽50ms
%     t4MHz=linspace(-T4MHz/2,T4MHz/2,n4MHz); %4MHz上对应得时间点
%     
%     k4MHz=blockBW/T4MHz;                    %调频斜率
%     signalTemp(1,:)=exp(1i*2*pi*frePointPos*t4MHz+1i*pi*k4MHz*t4MHz.^2);     %LFM信号
%     InterferSignal(1,:) = signalTemp(1,:).*sqrt(db2pow(PowerDiff)).*sqrt(50*10);
    %%
    M = 4; % Modulation order
    freqsep = blockBW/M; % Frequency separation (Hz)
    nsamp = 16; % Number of samples per symbol
    Fs = sampleRate; % Sample rate (Hz)
    N = ceil(SignalLen/nsamp);
    x = randi([0 M-1],N,1);
    y = fskmod(x,M,freqsep,nsamp,Fs);
    signalTemp = y.';
    InterferSignal(1,:) = signalTemp(1,1:SignalLen).*sqrt(db2pow(PowerDiff)).*sqrt(50);
    
%     specAnal = dsp.SpectrumAnalyzer('SampleRate',Fs);
%     specAnal(y)
%     [Pxif1, fif1] = pwelch(InterferSignal(1,1:length(InterferSignal)),[], [], [],SysSimulationPara.TerminalTxSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;

end