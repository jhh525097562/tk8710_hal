
function [SignalOut] = PeriodicInterference_SignalGen(SimPara,TMTXPara,GWRXPara,SignalLen,PerInterToneNum,Iter)
    
    s = RandStream('mt19937ar','Seed',Iter);
    RandStream.setGlobalStream(s);
    
    Freq_Centre = randi(GWRXPara.GWRxSampleRate,1) - GWRXPara.GWRxSampleRate/2;
    Periodic = SimPara.Periodic;
    for i=1:PerInterToneNum/2
        if Freq_Centre - Periodic*i < -GWRXPara.GWRxSampleRate/2
            Freq_Interfer(i) = GWRXPara.GWRxSampleRate/2 - abs(Freq_Centre - Periodic*i);
            Ampc(i) = 0;
        else
            Freq_Interfer(i) = Freq_Centre - Periodic*i;
            Ampc(i) = db2pow(-2*i);
        end
        
        if Freq_Centre + Periodic > GWRXPara.GWRxSampleRate/2
            Freq_Interfer(i+PerInterToneNum/2) = GWRXPara.GWRxSampleRate + Freq_Centre + Periodic*i;
            Ampc(i+PerInterToneNum/2) = 0;
        else
            Freq_Interfer(i+PerInterToneNum/2) = Freq_Centre + Periodic*i;
            Ampc(i+PerInterToneNum/2) = db2pow(-2*i);
        end
%         Ampc(i) = db2pow(-2*i);
%         Ampc(i+PerInterToneNum/2) = db2pow(-2*i);
    end
    Freq_Interfer(PerInterToneNum+1) = Freq_Centre;
    Ampc(PerInterToneNum+1) = 1;
    
    tmpO = 0:1:SignalLen-1;
    TestRxTemp = zeros(1,SignalLen);
    for i=1:PerInterToneNum+1
        TestRxTemp = TestRxTemp + sqrt(Ampc(i)).*exp(1i*2*pi*tmpO*Freq_Interfer(i)/TMTXPara.SampleRate);
    end
    
    SignalOut = (sqrt(db2pow(SimPara.Periodic_InterferencePower)).*TestRxTemp).'.*sqrt(50);
    
%     [Pxif1, fif1] = pwelch(TestRxTemp(1,1:length(TestRxTemp)),[], [], [],TMTXPara.SampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;

end