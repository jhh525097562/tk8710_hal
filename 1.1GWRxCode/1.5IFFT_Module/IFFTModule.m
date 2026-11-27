function [SigRxOut] = IFFTModule(SigRxIn,Antenna,IFFTLen)

SigRxOut = zeros(Antenna,IFFTLen);

SigRxOut = ifft(SigRxIn(1:Antenna,1:IFFTLen).')*IFFTLen;
SigRxOut = SigRxOut.';
SigRxOut = [SigRxOut(:,IFFTLen/2+1:end) SigRxOut(:,1:IFFTLen/2)];
end
