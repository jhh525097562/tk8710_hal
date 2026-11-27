function [UpLinkH,DownLinkH,GWRXSNR_REG,GWTXSNR_REG] = Uplink_DownLinkAH(GWRxAntennaNum,NumberFFT,FreqStartPos,ACMTxSignal,DataTxSignal,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,HTemp,PathAttenuation,noiseVar)
NumDiff = 1;
NumCarrier = 15;

s = RandStream('mt19937ar','Seed',8);
RandStream.setGlobalStream(s);
%% ACM发，Data通道收
Noise = (rand(GWRxAntennaNum,length(ACMTxSignal)) + (1j)*rand(GWRxAntennaNum,length(ACMTxSignal))) * sqrt(noiseVar/2);
GWRxDataChannelSingla = HTemp*ACMTxSignal.*sqrt(db2pow(PathAttenuation.')) + Noise;
%射频接收支路引起信道不互易分量
GWRxDataChannelSingla = GWRxDataChannelSingla.*GWAntennaRxPhaseNoReciprocal;
%% Data通道发，ACM收
%射频发射支路引起信道不互易分量
DataTxSignal = DataTxSignal.*GWAntennaTxPhaseNoReciprocal;
Noise = (rand(1,length(DataTxSignal)) + (1j)*rand(1,length(DataTxSignal))) * sqrt(noiseVar/2);
GWRxACMChannelSignal = HTemp.'*(DataTxSignal.*sqrt(db2pow(PathAttenuation.'))) + Noise;
%% 信道估计提取,硬件实现
for i=1:GWRxAntennaNum
    GWRxDataChannelSinglafft(i,:) = fft(GWRxDataChannelSingla(i,1:NumberFFT),NumberFFT);
    %         figure;plot(10*log10(abs(GWRxDataChannelSinglafft(i,:)).^2));
end

GWRxACMChannelSignalfft = fft(GWRxACMChannelSignal(1,1:NumberFFT),NumberFFT);
%     figure;plot(10*log10(abs(GWRxACMChannelSignalfft(1,:)).^2));
for i=1:GWRxAntennaNum
    GWRXSNR_REG(i) = RecipracalSNRALG(GWRxDataChannelSinglafft(i,FreqStartPos+1-8:FreqStartPos+15+8),i);
    GWTXSNR_REG(i) = RecipracalSNRALG(GWRxACMChannelSignalfft(1,FreqStartPos+1-8:FreqStartPos+15+8),i);
end

Cphase1=GWRxDataChannelSinglafft(:,(FreqStartPos+1):NumDiff:(FreqStartPos+1)+(NumCarrier-1)*NumDiff);
% GainDif = sqrt(db2pow(DataTxGain))./sqrt(db2pow(ACMTxGain));
% Cphase1 = Cphase1.*GainDif.';
UpLinkH=zeros(1,NumCarrier);
UpLinkH(1,1:2)=Cphase1(1,1:2);
UpLinkH(1,4:2:end)=Cphase1(1,4:2:end);
for j=3:2:NumCarrier
    UpLinkH(1,j)=Cphase1((j+1)/2,j);
end
DownLinkH= GWRxACMChannelSignalfft((FreqStartPos+1):NumDiff:(FreqStartPos+1)+(NumCarrier-1)*NumDiff);

end