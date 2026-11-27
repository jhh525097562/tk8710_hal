function [SigDownSampleRxTmp] = Downsampling(SigBaseNoiseRx,R)
    %%
    filterspec = 'Filter order and transition width';
    HalfbandFilterOrderNum =  10;
    FirHalfbandDeci = dsp.FIRHalfbandDecimator('Specification',filterspec,'FilterOrder',HalfbandFilterOrderNum,'TransitionWidth',1/4,'SampleRate',1);

    DownsamplingRateM= log2(R);
    M = length(SigBaseNoiseRx(:,1));
    for i4 = 1:M
        HalfbandDeciOut = SigBaseNoiseRx(i4,:);
        for i2 = 1:DownsamplingRateM
            tmpLen = ceil(length(HalfbandDeciOut)/2)*2;
            HalfbandDeciOut = [HalfbandDeciOut zeros(1,tmpLen-length(HalfbandDeciOut))]; % length must be even number
            HalfbandDeciOut = step(FirHalfbandDeci,HalfbandDeciOut.').';
        end
        SigDownSampleRxTmp(i4,1:length(HalfbandDeciOut)) = HalfbandDeciOut;
    end
    
end

