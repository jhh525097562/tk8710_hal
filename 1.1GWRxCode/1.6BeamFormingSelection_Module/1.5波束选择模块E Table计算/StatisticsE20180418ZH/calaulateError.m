function [ errorNumber, errorRate ] = calaulateError( Rx, Tx  )
%CALAULATEERROR ¼ÆËãÎóÂëÂÊ

fs = 32;
fs2 = 8;
seg1 = (764-256)/2*8;
seg2 = (764+256)/2*8;

RxSeg1 = Rx(:,1:seg1);
RxSeg2 = Rx(:,seg2+1:end);

dataReal1 = real(RxSeg1(:,2*fs2+1:2*fs2:end-fs2));
dataReal1(dataReal1>=0) = 1;
dataReal1(dataReal1<0) = -1;
dataImag1 = imag(RxSeg1(:,fs2+1:2*fs2:end-fs2));
dataImag1(dataImag1>=0) = 1;
dataImag1(dataImag1<0) = -1;

dataReal2 = real(RxSeg2(:,2*fs2+1:2*fs2:end-fs2));
dataReal2(dataReal2>=0) = 1;
dataReal2(dataReal2<0) = -1;
dataImag2 = imag(RxSeg2(:,fs2+1:2*fs2:end-fs2));
dataImag2(dataImag2>=0) = 1;
dataImag2(dataImag2<0) = -1;

TxSeg1 = Tx(:,1:(seg1*4));
TxSeg2 = Tx(:,(seg2*4+1):end);

TxSeg1Real = real(TxSeg1(:,2*fs+1:2*fs:end-fs));
TxSeg1Imag = imag(TxSeg1(:,fs+1:2*fs:end-fs));

TxSeg2Real = real(TxSeg2(:,2*fs+1:2*fs:end-fs));
TxSeg2Imag = imag(TxSeg2(:,fs+1:2*fs:end-fs));

errorNumber = sum(abs(round([TxSeg1Real TxSeg1Imag TxSeg2Real TxSeg2Imag]) - [dataReal1 dataImag1 dataReal2 dataImag2]),2)/2;

errorRate = errorNumber / 504;

end

