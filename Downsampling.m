function [SigDownSampleRxTmp] = Downsampling(SigBaseNoiseRx,GWRXPara)
    %%
    filterspec = 'Filter order and transition width';
    HalfbandFilterOrderNum =  10;
    FirHalfbandDeci = dsp.FIRHalfbandDecimator('Specification',filterspec,'FilterOrder',HalfbandFilterOrderNum,'TransitionWidth',1/4,'SampleRate',1);

    DownsamplingRateM= log2(GWRXPara.RFSampleRate/GWRXPara.GWRxSampleRate);
    GWRxPilotWindowNum = 1;
    
    GWRxLen = (GWRxPilotWindowNum+GWRXPara.GWRxDataWindowNum)*GWRXPara.GWRxFFTLen;
    SigDownSampleRxTmp = zeros(GWRXPara.AntennaNum,GWRxLen);
    for i4 = 1:GWRXPara.AntennaNum
    %     Noise = (randn(1,len) + (1j)*randn(1,len)) * sqrt(var/2);
        HalfbandDeciOut = SigBaseNoiseRx(i4,:);% + 1*Noise;
        for i2 = 1:DownsamplingRateM
            tmpLen = ceil(length(HalfbandDeciOut)/2)*2;
            HalfbandDeciOut = [HalfbandDeciOut zeros(1,tmpLen-length(HalfbandDeciOut))]; % length must be even number
            HalfbandDeciOut = step(FirHalfbandDeci,HalfbandDeciOut.').';
        end
        SigDownSampleRxTmp(i4,1:length(HalfbandDeciOut)) = HalfbandDeciOut;
    end
    
end

