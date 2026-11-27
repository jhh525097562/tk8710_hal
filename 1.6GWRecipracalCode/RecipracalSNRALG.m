function [SNR_o] = RecipracalSNRALG(FFTResult,Antenna)

ANoiseEsum = 0;

for i=1:8
    ANoiseEsum = ANoiseEsum + abs(FFTResult(1,i))^2 + abs(FFTResult(1,i+15+8))^2;
end
MeanANoiseEsum =ANoiseEsum/16;

SignalEsum = abs(FFTResult(8+2*Antenna - 1))^2;

SNR_o = 10*log10(SignalEsum/MeanANoiseEsum);
end