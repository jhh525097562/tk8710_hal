function [InterferToneSignal] = InterferenceToneGen(SimPara,GWRXPara,TMTXPara,SignalLen,Iter)

    s = RandStream('mt19937ar','Seed',Iter);
    RandStream.setGlobalStream(s);
    InterferenceNum = SimPara.SingleToneInterferenceNum;
    Freq_Interfer = randi(GWRXPara.GWRxSampleRate,[1 InterferenceNum]) - GWRXPara.GWRxSampleRate/2;
    %     Freq_Interfer = 15067:1590:15067+(InterferenceNum-1)*1590;
    tmpO = 0:1:SignalLen-1;
    TestRxTemp = zeros(1,SignalLen);
    for i=1:InterferenceNum
        TestRxTemp = TestRxTemp + exp(1i*2*pi*tmpO*Freq_Interfer(i)/TMTXPara.SampleRate);
    end
    InterferToneSignal = (sqrt(db2pow(SimPara.SingleToneInterferencePower)).*TestRxTemp).'.*sqrt(50);
    
    
%     [Pxif1, fif1] = pwelch(TestRxTemp(1,1:length(TestRxTemp)),[], [], [],SysSimulationPara.TerminalTxSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;

end