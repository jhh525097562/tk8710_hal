clear all;
close all;

load('GWRxACMChannelSignalfft.mat');
load('GWRxDataChannelSinglafft.mat');

GWRxAntennaNum = 8;
FreqStartPos = 100;

for i=1:GWRxAntennaNum
    GWRXSNR_REG(i) = RecipracalSNRALG(GWRxDataChannelSinglafft(i,FreqStartPos+1-8:FreqStartPos+15+8),i);
    GWTXSNR_REG(i) = RecipracalSNRALG(GWRxACMChannelSignalfft(1,FreqStartPos+1-8:FreqStartPos+15+8),i);
    figure;plot(10*log10(abs(GWRxDataChannelSinglafft(i,:)).^2));
end
figure;plot(10*log10(abs(GWRxACMChannelSignalfft(1,:)).^2));