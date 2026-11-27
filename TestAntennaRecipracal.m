function [CalibrateFactor] = TestAntennaRecipracal(GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,SelectAntenna,GWRxAntennaNum,SampleRate)


s = RandStream('mt19937ar','Seed',5);
RandStream.setGlobalStream(s);

SnrSamplingRate = 20;
noiseVar = 1/10^(SnrSamplingRate/10);    % 噪声方差，接收端以符号速率为采样速度，发送信号功率为1，所以噪声方差也是噪声功率


NumberFFT=512*(SampleRate/125000); %FFT个数
Freqdif = SampleRate/NumberFFT;

FreqStartPos = 10;
NumDiff=1; % 载波间隔点数 两点间隔125000/1024
NumCarrier=(GWRxAntennaNum-1)*2-1;%

offset = Freqdif*FreqStartPos;
for i=1:NumCarrier
    Freq(i) =offset+Freqdif*i;
end
%% Select通道发，其他Data通道收
%信号生成
P=[-1.02324779750120,-2.58039305874270,-2.78523111484624,0.363193019902249,1.29378952938372,2.13681484724016,-1.00220928762122,0.207595752893231,-3.02187936892107,0.936349644728564,-2.36334565934660,0.126400964528446,-0.0828443127029830,1.02527778844203,0.371577395760742];
% P = zeros(1,15);
for i = 1:NumberFFT
    t(i) = 1/SampleRate * (i - 1); %观测点
end
SelectDataTxSignal = zeros(1,NumberFFT);
for i=1:NumCarrier
    Tone(i,:) = cos(2*pi*Freq(i)*t-P(i)) + sin(2*pi*Freq(i)*t-P(i))*1i;
    SelectDataTxSignal = SelectDataTxSignal + Tone(i,:); %设定5个复数正弦发射信号
end

HTemp = (randn(GWRxAntennaNum-1,1) + 1i* randn(GWRxAntennaNum-1,1));
for i = 1:1
    HTemp(:,i) = sqrt(GWRxAntennaNum-1)*HTemp(:,i)/sqrt(sum(abs(HTemp(:,i).^2))); %归一化当前信号的信道响应
end
Noise = (rand(GWRxAntennaNum-1,length(SelectDataTxSignal)) + (1j)*rand(GWRxAntennaNum-1,length(SelectDataTxSignal))) * sqrt(noiseVar/2);
GWRxDataChannelSingla = HTemp*SelectDataTxSignal + Noise;

%射频接收支路引起信道不互易分量
TempGWAntennaRxPhaseNoReciprocal = [GWAntennaRxPhaseNoReciprocal(1:SelectAntenna-1);GWAntennaRxPhaseNoReciprocal(SelectAntenna+1:end)];
GWRxDataChannelSingla = GWRxDataChannelSingla.*TempGWAntennaRxPhaseNoReciprocal;
% GWRxDataChannelSingla = GWRxDataChannelSingla./GWAntennaRxPhaseNoReciprocal;
%% 其他Data通道发，SelectData收
DataTxSignal = zeros(GWRxAntennaNum-1,NumberFFT);
for i=1:NumCarrier
    Tone(i,:) = cos(2*pi*Freq(i)*t) + sin(2*pi*Freq(i)*t)*1i;
end
for i=1:GWRxAntennaNum-1
    if i==1
        DataTxSignal(i,:) = Tone(1,:);
        for j=1:(GWRxAntennaNum-2)
            DataTxSignal(i,:) = DataTxSignal(i,:) + Tone(j*2,:);
        end
    else
        DataTxSignal(i,:) = Tone(2*i-1,:);
    end
end
%射频发射支路引起信道不互易分量
TempGWAntennaTxPhaseNoReciprocal = [GWAntennaTxPhaseNoReciprocal(1:SelectAntenna-1);GWAntennaTxPhaseNoReciprocal(SelectAntenna+1:end)];
DataTxSignal = DataTxSignal.*TempGWAntennaTxPhaseNoReciprocal;

Noise = (rand(1,length(DataTxSignal)) + (1j)*rand(1,length(DataTxSignal))) * sqrt(noiseVar/2);
SelectDataChannelSignal = HTemp.'*DataTxSignal + Noise;

for i=1:GWRxAntennaNum-1
    GWRxDataChannelSinglafft(i,:) = fft(GWRxDataChannelSingla(i,1:NumberFFT),NumberFFT);
%     figure;plot(10*log10(abs(GWRxDataChannelSinglafft(i,:)).^2));
end

SelectDataChannelSignalfft = fft(SelectDataChannelSignal(1,1:NumberFFT),NumberFFT);
% figure;plot(10*log10(abs(GWRxACMChannelSignalfft(1,:)).^2));

Cphase1=GWRxDataChannelSinglafft(:,(FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);
UplinkC=zeros(1,NumCarrier);
UplinkC(1,1:2)=Cphase1(1,1:2);
UplinkC(1,4:2:end)=Cphase1(1,4:2:end);
for j=3:2:NumCarrier
   UplinkC(1,j)=Cphase1((j+1)/2,j);
end

DownlnkC= SelectDataChannelSignalfft((FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);

UplinkC = UplinkC.*(cos(P(1:NumCarrier)) + sin(P(1:NumCarrier))*1i);
Cphase2(1,:)=(UplinkC(1,:)./DownlnkC(1,:));
% Cphase2(1,:)=(UplinkC(1,:)./conj(DownlnkC(1,:)));
for j=3:2:NumCarrier
    temp2((j-1)/2) = (Cphase2(1,2)/Cphase2(1,1));
    temp1((j-1)/2) = Cphase2(1,j)/Cphase2(1,j-1);
    temp=(Cphase2(1,j)/Cphase2(1,1))/(Cphase2(1,j-1)/(Cphase2(1,2)));
    CalibrateFactor(1,(j-1)/2)=temp;
end



end