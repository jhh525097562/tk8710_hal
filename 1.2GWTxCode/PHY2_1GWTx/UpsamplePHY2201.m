function   FirhalfbandOut = UpsamplePHY2201(sigTxFilt,SamplingRateTx,Fs)

%     filterspec = 'Filter order and transition width';
%     HalfbandFilterOrder =  8;
%     FirHalfbandInterp = dsp.FIRHalfbandInterpolator('Specification',filterspec,'FilterOrder',HalfbandFilterOrder,'TransitionWidth',3/4,'SampleRate',1);
%     
%     tmpU = log2(SamplingRateTx/Fs);
%     for it=1:length(sigTxFilt(:,1))
%     filter_o = sigTxFilt(it,:);
%     FirHalfbandOutR = real(filter_o);
%     FirHalfbandOutI = imag(filter_o);
%     for i2 = 1:tmpU
%         FirHalfbandInR = [FirHalfbandOutR zeros(1,HalfbandFilterOrder/4)];
%         FirHalfbandInI = [FirHalfbandOutI zeros(1,HalfbandFilterOrder/4)];
%         tmpR = step(FirHalfbandInterp,FirHalfbandInR.').';
%         tmpI = step(FirHalfbandInterp,FirHalfbandInI.').';
%         FirHalfbandOutR = tmpR(1,HalfbandFilterOrder/2:end);
%         FirHalfbandOutI = tmpI(1,HalfbandFilterOrder/2:end);
%     end
%     FirhalfbandOut(it,:) = FirHalfbandOutR + 1i*FirHalfbandOutI;
%     end
    

m = log2(SamplingRateTx/Fs);

% design the filter
% tx_Hd_match = tx_match_filter_design(4);
% Hd_halfband = tx_halfband_design;
% tmpF2 = Hd_halfband.Numerator;
tmpF2 = [0,0,0,-0.000976562500000000,0,0.00390625000000000,0,-0.0126953125000000,0,0.0341796875000000,0,-0.0849609375000000,0,0.310546875000000,0.500000000000000,0.310546875000000,0,-0.0849609375000000,0,0.0341796875000000,0,-0.0126953125000000,0,0.00390625000000000,0,-0.000976562500000000,0,0,0];
HBFilter = tmpF2/sum(tmpF2);
b  = HBFilter*2; %



% Hd_halfband = tx_halfband_design;
% HBFilter = Hd_halfband.Numerator;
% b = HBFilter*2;

for it=1:length(sigTxFilt(:,1))
%     in_osr1 = conv(sigTxFilt(it,:), tx_Hd_match.Numerator);
%     in_osr2 = in_osr1(33:end-32);
%     filter_o = in_osr2;
    filter_o = sigTxFilt(it,:);

    for ck0 = 1:m
        temp = zeros(1,length(filter_o)*2);
        temp(1:2:end) = filter_o;
        filter_o = conv(temp, b);
    end
    out = filter_o;

    delay = length(out) - length(sigTxFilt(it,:))*2^m;
    FirhalfbandOut(it,:) = out(delay/2+1 :end-delay/2); 
end
end