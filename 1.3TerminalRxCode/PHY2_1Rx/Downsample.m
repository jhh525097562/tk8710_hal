function HalfbandDeciOut =  Downsample(sigRx,SamplingRateTx,Fs,HBFilter)

%     filterspec = 'Filter order and transition width';
%     HalfbandFilterOrderNum =  10;
%     FirHalfbandDeci = dsp.FIRHalfbandDecimator('Specification',filterspec,'FilterOrder',HalfbandFilterOrderNum,'TransitionWidth',1/4,'SampleRate',1);
%     BaseUpsamplingRateRx = 8;
%     tmpU = log2(SamplingRateTx/Fs);
%     HalfbandDeciOut=sigRx.';
%     for i2 = 1:tmpU
%         tmpLen = ceil(length(HalfbandDeciOut)/2)*2;
%         HalfbandDeciOut = [HalfbandDeciOut zeros(1,tmpLen-length(HalfbandDeciOut))]; % length must be even number
%         HalfbandDeciOut = step(FirHalfbandDeci,HalfbandDeciOut.').';
%     end
    
b  = HBFilter; 
in_matrix = sigRx;
m = log2(SamplingRateTx/Fs);

for ck0 = 1:m
    temp = conv(in_matrix, b);
    in_matrix = temp(1:2:end);
end
out = in_matrix;
% delay = length(out) - LenSigTx;
HalfbandDeciOut = out(15:end); 

end