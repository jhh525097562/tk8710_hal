% function [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactorOut_REG,GWRXSNR_REG,GWTXSNR_REG] = GWAntennaRecipracal(SimPara,GWRXPara)
% 
% GWRxAntennaNum = SimPara.AntennaNum;
% 
% if GWRxAntennaNum==1
%     GWAntennaRxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
%     GWAntennaTxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
%     CalibrateFactorOut_REG(1:SimPara.AntennaNum) = ones(1,GWRxAntennaNum);
%     GWRXSNR_REG = 40;
%     GWTXSNR_REG = 40;
% else
%     s = RandStream('mt19937ar','Seed',5);
%     RandStream.setGlobalStream(s);
%     
%     SnrSamplingRate = 20;
%     noiseVar = 1/10^(SnrSamplingRate/10);    % 噪声方差，接收端以符号速率为采样速度，发送信号功率为1，所以噪声方差也是噪声功率
%     
%     GWAntennaRxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
%     GWAntennaTxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
%     for i=1:GWRxAntennaNum
%         AmpcRx(i) = 1+0.03*randi([-5 5],1,1);
%         AmpcTx(i) = 1+0.03*randi([-5 5],1,1);
%         GWAntennaRxPhaseNoReciprocal(i,1) = AmpcRx(i)*GWAntennaRxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaRxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
%         GWAntennaTxPhaseNoReciprocal(i,1) = AmpcTx(i)*GWAntennaTxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaTxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
%     end
%     
%     %RX TX NoRecipracal==1
%     if SimPara.RecipracalCaliFlag==0
%         GWAntennaRxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
%         GWAntennaTxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
%     end
%     
%     %% ACM发，Data通道收
%     %信号生成
%     NumberFFT=512*(GWRXPara.GWRxSampleRate/125000); %FFT个数
%     Freqdif = GWRXPara.GWRxSampleRate/NumberFFT;
%     % FreqStartPos = 100;
%     FreqStartPos = 10;
%     NumDiff=1; % 载波间隔点数 两点间隔125000/1024
%     NumCarrier=GWRxAntennaNum*2-1;  %number of carrier 2*7-1  %后续将减少到8个 test_slot0508_22 test_channel3_noise18
%     offset = Freqdif*FreqStartPos;
%     for i=1:NumCarrier
%         Freq(i) =offset+Freqdif*i;
%     end
%     P=[-1.02324779750120,-2.58039305874270,-2.78523111484624,0.363193019902249,1.29378952938372,2.13681484724016,-1.00220928762122,0.207595752893231,-3.02187936892107,0.936349644728564,-2.36334565934660,0.126400964528446,-0.0828443127029830,1.02527778844203,0.371577395760742];
%     % P = zeros(1,15);
%     for i = 1:NumberFFT
%         t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %观测点
%     end
%     ACMTxSignal = zeros(1,NumberFFT);
%     for i=1:NumCarrier
%         Tone(i,:) = cos(2*pi*Freq(i)*t-P(i)) + sin(2*pi*Freq(i)*t-P(i))*1i;
%         ACMTxSignal = ACMTxSignal + Tone(i,:); %设定5个复数正弦发射信号
%     end
%     
%     HTemp = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
%     for i = 1:1
%         HTemp(:,i) = sqrt(GWRxAntennaNum)*HTemp(:,i)/sqrt(sum(abs(HTemp(:,i).^2))); %归一化当前信号的信道响应
%     end
%     Noise = (rand(GWRxAntennaNum,length(ACMTxSignal)) + (1j)*rand(GWRxAntennaNum,length(ACMTxSignal))) * sqrt(noiseVar/2);
%     GWRxDataChannelSingla = HTemp*ACMTxSignal + Noise;
%     
%     %射频接收支路引起信道不互易分量
%     GWRxDataChannelSingla = GWRxDataChannelSingla.*GWAntennaRxPhaseNoReciprocal;
%     % GWRxDataChannelSingla = GWRxDataChannelSingla./GWAntennaRxPhaseNoReciprocal;
%     %% Data通道发，ACM收
%     DataTxSignal = zeros(GWRxAntennaNum,NumberFFT);
%     for i=1:NumCarrier
%         Tone(i,:) = cos(2*pi*Freq(i)*t) + sin(2*pi*Freq(i)*t)*1i;
%     end
%     for i=1:GWRxAntennaNum
%         if i==1
%             DataTxSignal(i,:) = Tone(1,:);
%             for j=1:(GWRxAntennaNum-1)
%                 DataTxSignal(i,:) = DataTxSignal(i,:) + Tone(j*2,:);
%             end
%         else
%             DataTxSignal(i,:) = Tone(2*i-1,:);
%         end
%     end
%     %射频发射支路引起信道不互易分量
%     DataTxSignal = DataTxSignal.*GWAntennaTxPhaseNoReciprocal;
%     % DataTxSignal = DataTxSignal./GWAntennaTxPhaseNoReciprocal;
%     
%     Noise = (rand(1,length(DataTxSignal)) + (1j)*rand(1,length(DataTxSignal))) * sqrt(noiseVar/2);
%     GWRxACMChannelSignal = HTemp.'*DataTxSignal + Noise;
%     
%     %% 校准因子计算
%     for i=1:GWRxAntennaNum
%         GWRxDataChannelSinglafft(i,:) = fft(GWRxDataChannelSingla(i,1:NumberFFT),NumberFFT);
%         %     figure;plot(10*log10(abs(GWRxDataChannelSinglafft(i,:)).^2));
%         GWRXSNR_REG(i) = (min(20*log10(abs(GWRxDataChannelSinglafft(i,(FreqStartPos+2):(FreqStartPos+2)+(NumCarrier-1)*NumDiff))))) ...
%         - (10*log10((abs(GWRxDataChannelSinglafft(i,(FreqStartPos+1)))^2 + abs(GWRxDataChannelSinglafft(i,(FreqStartPos+3)+(NumCarrier-1)*NumDiff))^2)/2));
%     end
%     
%     GWRxACMChannelSignalfft = fft(GWRxACMChannelSignal(1,1:NumberFFT),NumberFFT);
%     % figure;plot(10*log10(abs(GWRxACMChannelSignalfft(1,:)).^2));
%     ANoise=(10*log10((abs(GWRxACMChannelSignalfft(1,(FreqStartPos+1)))^2 + abs(GWRxACMChannelSignalfft(1,(FreqStartPos+3)+(NumCarrier-1)*NumDiff))^2)/2));
%     for i=1:2*NumDiff:NumCarrier
%         GWTXSNR_REG((i+1)/2) = 20*log10(abs(GWRxACMChannelSignalfft(1,FreqStartPos+1+i))) - ANoise;
%     end
%     
%     Cphase1=GWRxDataChannelSinglafft(:,(FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);
%     UplinkC=zeros(1,NumCarrier);
%     UplinkC(1,1:2)=Cphase1(1,1:2);
%     UplinkC(1,4:2:end)=Cphase1(1,4:2:end);
%     for j=3:2:NumCarrier
%         UplinkC(1,j)=Cphase1((j+1)/2,j);
%     end
%     
%     DownlnkC= GWRxACMChannelSignalfft((FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);
%     
%     UplinkC = UplinkC.*(cos(P(1:NumCarrier)) + sin(P(1:NumCarrier))*1i);
%     Cphase2(1,:)=(UplinkC(1,:)./DownlnkC(1,:));
%     % Cphase2(1,:)=(UplinkC(1,:)./conj(DownlnkC(1,:)));
%     for j=3:2:NumCarrier
%         temp2((j-1)/2) = (Cphase2(1,2)/Cphase2(1,1));
%         temp1((j-1)/2) = Cphase2(1,j)/Cphase2(1,j-1);
%         temp=(Cphase2(1,j)/Cphase2(1,1))/(Cphase2(1,j-1)/(Cphase2(1,2)));
%         CalibrateFactor(1,(j-1)/2)=temp;
%     end
%     % for i=1:8
%     %         %Test
%     %     CalibrateFactorTemp(i) = Cphase2(1,2*i-1);
%     % end
%     % CalibrateFactorOut = CalibrateFactorTemp;
%     
%     %% Sim校准不准确性
%     temp = 0.01*SimPara.CalibrateFactorDiff;
%     CalibrateFactorDiff = (temp*randi([-5 5],1,GWRxAntennaNum-1) + temp*randi([-5 5],1,GWRxAntennaNum-1)*1i);
%     x1=abs(CalibrateFactorDiff);
%     y1=angle(CalibrateFactorDiff);
%     x2=abs(CalibrateFactor);
%     y2=angle(CalibrateFactor);
%     
%     % CalibrateFactor = CalibrateFactor + CalibrateFactorDiff;
%     x3=abs(CalibrateFactor);
%     y3=angle(CalibrateFactor);
%     PhaseDiff = sum(abs(y3-y2))
%     AmpcDiff = sum(abs(x3-x2))
%     
%     CalibrateFactorOut_REG(1) = 1;
%     CalibrateFactorOut_REG(2:GWRxAntennaNum) = CalibrateFactor;
%     %校准因子验证
%     [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactorOut_REG,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal);
% end
% end
% 
% function [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactor,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal)
% 
%     % % Dsp 数据导入
%     % close all;
%     % GWRxAntennaNum = 8;
%     % NumberFFT = 512;
%     % NumCarrier  = 15;
%     % NumDiff = 1;
%     %
%     % file_name=['H:\网关状态信息显示上位机\data\data' num2str(1)];
%     % fid0 = fopen(file_name,'r');
%     % datain_one = fread(fid0','float')';fclose(fid0);
%     % P5 = (33000+1000+200*30+300*30)/4;
%     % % P5 = (33000+1000)/4;
%     % RxSignal0 = datain_one(P5+1:P5+25000*9*2);
%     % RxSignal0 = reshape(RxSignal0,[18 25000]);
%     % for i=1:9
%     %     RxSignal1(i,:) = RxSignal0(2*(i-1)+1,:) + RxSignal0(2*i,:)*1i;
%     % end
%     % figure;plot(real(RxSignal1(9,:)));
%     % figure;plot(real(RxSignal1(1,:)));
%     %
%     % GWRxDataChannelSingla = RxSignal1(1:8,512+61:60+512*2);
%     % GWRxACMChannelSignal = RxSignal1(9,1:512);
%    
%     %% Test CalibrateFactorOut
%     GWRxAntennaNum = GWRXPara.AntennaNum;
%     SnrSamplingRate = 20;
%     noiseVar = 1/10^(SnrSamplingRate/10);    % 噪声方差，接收端以符号速率为采样速度，发送信号功率为1，所以噪声方差也是噪声功率
%     NumberFFT1 = 8192;
%     FreqPoint = 500;
%     TxFreq = FreqPoint*GWRXPara.GWRxSampleRate/NumberFFT1;
%     for i = 1:NumberFFT1
%         t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %观测点
%     end
%     UserTxSignal = zeros(1,NumberFFT1);
%     UserTxSignal = cos(2*pi*TxFreq*t) + sin(2*pi*TxFreq*t)*1i;
%     
%     HTemp1 = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
%     for i = 1:1
%         HTemp1(:,i) = sqrt(GWRxAntennaNum)*HTemp1(:,i)/sqrt(sum(abs(HTemp1(:,i).^2))); %归一化当前信号的信道响应
%     end
%     Noise = (rand(GWRxAntennaNum,length(UserTxSignal)) + (1j)*rand(GWRxAntennaNum,length(UserTxSignal))) * sqrt(noiseVar/2);
%     GWRxSignal = HTemp1*UserTxSignal + Noise;
%     %射频接收支路引起信道不互易分量
%     GWRxSignal = GWRxSignal.*GWAntennaRxPhaseNoReciprocal;
%     for i=1:GWRxAntennaNum
%         GWRxSignalfft(i,:) = fft(GWRxSignal(i,1:NumberFFT1),NumberFFT1);
%         %     figure;plot(10*log10(abs(GWRxSignalfft(i,:)).^2));
%     end
%     GWRxAH = GWRxSignalfft(:,FreqPoint+1);
%     
%     %
%     for i = 1:NumberFFT1
%         t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %观测点
%     end
%     GWTxSignal0 = zeros(GWRxAntennaNum,NumberFFT1);
%     for i=1:GWRxAntennaNum
%         for j=1:NumberFFT1
%             if j>=1024*(i-1)+1 && j<=1024*i
%                 GWTxSignal0(i,j) = cos(2*pi*TxFreq*t(j)) + sin(2*pi*TxFreq*t(j))*1i;
%             end
%         end
%     end
%     GWRxAH = GWRxAH./sqrt(sum(abs(GWRxAH).^2));
%     %使用实际信道进行下行BF
%     % GWRxAH = HTemp1.*GWAntennaRxPhaseNoReciprocal;
%     
%     BF = GWRxAH';
%     BFOutMax = max(abs(BF).^2);
%     for i=1:GWRxAntennaNum
%         TxAmpc(1,i) = sqrt(BFOutMax)/abs(BF(i)).^2;
%         BF(i) = BF(i)*TxAmpc(i);
%     end
%     
%     BFOut = BF.*CalibrateFactor;
%     GWTxSignal = GWTxSignal0.*BFOut.';
%     
%     %射频发射支路引起信道不互易分量
%     GWTxSignal = GWTxSignal.*GWAntennaTxPhaseNoReciprocal;
%     
%     Noise = (rand(1,length(GWTxSignal)) + (1j)*rand(1,length(GWTxSignal))) * sqrt(noiseVar/2);
%     UserRxSignal = HTemp1.'*GWTxSignal + 0*Noise;
%     % figure;plot(real(UserRxSignal))
%     % CalibrateFactorOut = ones(1,8);
% end


function [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactorOut_REG,GWRXSNR_REG,GWTXSNR_REG] = GWAntennaRecipracal(SimPara,GWRXPara)

GWRxAntennaNum = SimPara.AntennaNum;

global P;
P =[-1.02324779750120,-2.58039305874270,-2.78523111484624,0.363193019902249,1.29378952938372,2.13681484724016,-1.00220928762122,0.207595752893231,-3.02187936892107,0.936349644728564,-2.36334565934660,0.126400964528446,-0.0828443127029830,1.02527778844203,0.371577395760742];
global NumCarrier;
NumCarrier = 15;

if GWRxAntennaNum==1
    GWAntennaRxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    GWAntennaTxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    CalibrateFactorOut_REG(1:SimPara.AntennaNum) = ones(1,GWRxAntennaNum);
    GWRXSNR_REG = 40;
    GWTXSNR_REG = 40;
else
%     TL = randi(100);
    s = RandStream('mt19937ar','Seed',8);%8
    RandStream.setGlobalStream(s);
    
    SnrSamplingRate = 30;%35
    TmpSNR = SnrSamplingRate-35;
    noiseVar = 1/10^(TmpSNR/10);    % 噪声方差，接收端以符号速率为采样速度，发送信号功率为1，所以噪声方差也是噪声功率
    
    GWAntennaRxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    GWAntennaTxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i=1:GWRxAntennaNum
        AmpcRx(i) = 1+0.03*randi([-5 5],1,1);
        AmpcTx(i) = 1+0.03*randi([-5 5],1,1);
        GWAntennaRxPhaseNoReciprocal(i,1) = AmpcRx(i)*GWAntennaRxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaRxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
        GWAntennaTxPhaseNoReciprocal(i,1) = AmpcTx(i)*GWAntennaTxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaTxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
    end
    
    %RX TX NoRecipracal==1
    if SimPara.RecipracalCaliFlag==0
        GWAntennaRxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
        GWAntennaTxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
    end
    

    %% 信号生成
    SelectACMAntenna = 9;
    NumberFFT=512; %FFT个数
    Freqdif = GWRXPara.GWRxSampleRate/NumberFFT;
    % FreqStartPos = 100;
    FreqStartPos = 100;
%     NumDiff=1; % 载波间隔点数 两点间隔125000/1024
%     NumCarrier=GWRxAntennaNum*2-1;  %number of carrier 2*7-1
    NumCarrier=15;  %number of carrier 2*7-1
    offset = Freqdif*FreqStartPos;
    for i=1:NumCarrier
        Freq(i) =offset+Freqdif*(i-1);
    end
%     PathAttenuation = [0  -10  -20  0  -5  -3  -2  -3];
    PathAttenuation = [0  0  0  0  0  0  0  0];
%     ACMTxGain = [0 3  5  9  2  1 0 8];
    ACMTxGain = [0 0  0  0  0  0 0 0];
%     DataTxGain = [4 5  6  2  0  4 7 2];
%     DataTxGain = [5 5  5  5  5  5 5 5];
%     DataTxGain = [20 20  20  20  20  20 20 20];
    DataTxGain = [0 0  0  0  0  0 0 0];
    GainDif = sqrt(db2pow(DataTxGain))./sqrt(db2pow(ACMTxGain));
    HTemp = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i = 1:GWRxAntennaNum
        HTemp(i,1) = HTemp(i,1)/sqrt(sum(abs(HTemp(i,1).^2))); %归一化当前信号的信道响应
    end
    
    %% 校准信号生成
   [ACMTxSignal,DataTxSignal] = RecipracalSignalGenerate(GWRxAntennaNum,SelectACMAntenna,GWRXPara.GWRxSampleRate,NumberFFT,Freq,P,sqrt(db2pow(ACMTxGain)),sqrt(db2pow(DataTxGain)));
   
   %% 信道估计获取
    [UpLinkH,DownLinkH,GWRXSNR_REG,GWTXSNR_REG] = Uplink_DownLinkAH(GWRxAntennaNum,NumberFFT,FreqStartPos,ACMTxSignal,DataTxSignal,GWAntennaRxPhaseNoReciprocal,...
        GWAntennaTxPhaseNoReciprocal,HTemp,PathAttenuation,noiseVar);

    [CalibrateFactor] = GWRecipracalALG(UpLinkH,DownLinkH,GainDif);
    
    if SelectACMAntenna<=8
            %% 校准信号生成
       [ACMTxSignal,DataTxSignal] = RecipracalSignalGenerate(GWRxAntennaNum,SelectACMAntenna-1,GWRXPara.GWRxSampleRate,NumberFFT,Freq,P,ACMTxGain,DataTxGain);

       %% 信道估计获取
        [UpLinkH1,DownLinkH1,GWRXSNR_REG,GWTXSNR_REG] = Uplink_DownLinkAH(GWRxAntennaNum,NumberFFT,FreqStartPos,ACMTxSignal,DataTxSignal,GWAntennaRxPhaseNoReciprocal,...
            GWAntennaTxPhaseNoReciprocal,HTemp,PathAttenuation,noiseVar);

        [CalibrateFactor1] = GWRecipracalALG(UpLinkH1,DownLinkH1,GainDif);
        CalibrateFactor(SelectACMAntenna-1) = CalibrateFactor1(SelectACMAntenna-1);
    end
%     CalibrateFactorOut_REG(1) = abs(temp2(1))*1;
    CalibrateFactorOut_REG(1) = 1;
    CalibrateFactorOut_REG(2:GWRxAntennaNum) = CalibrateFactor;
    %% 校准因子验证
    % Sim校准不准确性
    for iter=1:10
        phasedif = 0;
        AmpcDif = 0;
%         temp = db2pow(2*SimPara.CalibrateFactorDiff*rand(1,SimPara.AntennaNum-1) - SimPara.CalibrateFactorDiff);
        temp = db2pow(2*AmpcDif*rand(1,SimPara.AntennaNum-1) - AmpcDif);
        p = ((2*phasedif*rand(1,SimPara.AntennaNum-1) - phasedif)./180).*pi;
        CalibrateFactorDiff =  sqrt(temp).*(cos(p) + sin(p)*1i);
        CalibrateFactor1 = CalibrateFactorOut_REG;
        CalibrateFactor1(2:end) = CalibrateFactorOut_REG(2:end).*CalibrateFactorDiff;
        CalibrateFactorTmp(:,iter) = CalibrateFactor1;

        [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactor1,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,iter);
        
        for i=1:GWRxAntennaNum-1
            Phase1(i,iter) = phase(dot(UserRxSignal(1+(i-1)*1024:i*1024),UserRxSignal(1+i*1024:(i+1)*1024)));
            Ampc1(i,iter) = abs(mean(abs(UserRxSignal(1+(i-1)*1024:i*1024)))-mean(abs(UserRxSignal(1+i*1024:(i+1)*1024))));
            
%             Phase2(i,iter) = phase(dot(UserRxSignal(1+(i-1)*1024+8192*2:i*1024+8192*2),UserRxSignal(1+i*1024+8192*2:(i+1)*1024+8192*2)));
%             Ampc2(i,iter) = abs(mean(abs(UserRxSignal(1+(i-1)*1024+8192*2:i*1024+8192*2)))-mean(abs(UserRxSignal(1+i*1024+8192*2:(i+1)*1024+8192*2))));
        end
    end
%     mean(abs(Phase1)) - pi
%     mean(abs(Ampc1))
    for i=1:SimPara.AntennaNum-1
        Var_CaliFactor_Phase(i) = var(phase(CalibrateFactorTmp(i+1,:)));
        Var_CaliFactor_Ampc(i) = var(abs(CalibrateFactorTmp(i+1,:)));
        Var_CaliSignal_Phase(i) = var((abs(Phase1(i,:))-pi));
        Var_CaliSignal_Ampc(i) = var(abs(Ampc1(i,:)));
    end
    Var_CaliFactor_Phase
    Var_CaliFactor_Ampc
    Var_CaliSignal_Phase
    Var_CaliSignal_Ampc
%     figure;plot(real(UserRxSignal))
end
end

function [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactor,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,iter)

    % % Dsp 数据导入
    % close all;
    % GWRxAntennaNum = 8;
    % NumberFFT = 512;
    % NumCarrier  = 15;
    % NumDiff = 1;
    %
    % file_name=['H:\网关状态信息显示上位机\data\data' num2str(1)];
    % fid0 = fopen(file_name,'r');
    % datain_one = fread(fid0','float')';fclose(fid0);
    % P5 = (33000+1000+200*30+300*30)/4;
    % % P5 = (33000+1000)/4;
    % RxSignal0 = datain_one(P5+1:P5+25000*9*2);
    % RxSignal0 = reshape(RxSignal0,[18 25000]);
    % for i=1:9
    %     RxSignal1(i,:) = RxSignal0(2*(i-1)+1,:) + RxSignal0(2*i,:)*1i;
    % end
    % figure;plot(real(RxSignal1(9,:)));
    % figure;plot(real(RxSignal1(1,:)));
    %
    % GWRxDataChannelSingla = RxSignal1(1:8,512+61:60+512*2);
    % GWRxACMChannelSignal = RxSignal1(9,1:512);
    s = RandStream('mt19937ar','Seed',iter);
    RandStream.setGlobalStream(s);
    %% Test CalibrateFactorOut
    GWRxAntennaNum = GWRXPara.AntennaNum;
    SnrSamplingRate = 20;
    noiseVar = 1/10^(SnrSamplingRate/10);    % 噪声方差，接收端以符号速率为采样速度，发送信号功率为1，所以噪声方差也是噪声功率
    NumberFFT1 = 8192;
    FreqPoint = 500;
    TxFreq = FreqPoint*GWRXPara.GWRxSampleRate/NumberFFT1;
    for i = 1:NumberFFT1
        t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %观测点
    end
    UserTxSignal = zeros(1,NumberFFT1);
    UserTxSignal = cos(2*pi*TxFreq*t) + sin(2*pi*TxFreq*t)*1i;
    
    HTemp1 = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i = 1:1
        HTemp1(:,i) = sqrt(GWRxAntennaNum)*HTemp1(:,i)/sqrt(sum(abs(HTemp1(:,i).^2))); %归一化当前信号的信道响应
    end
    Noise = (rand(GWRxAntennaNum,length(UserTxSignal)) + (1j)*rand(GWRxAntennaNum,length(UserTxSignal))) * sqrt(noiseVar/2);
    GWRxSignal = HTemp1*UserTxSignal + Noise;
    %射频接收支路引起信道不互易分量
    GWRxSignal = GWRxSignal.*GWAntennaRxPhaseNoReciprocal;
    for i=1:GWRxAntennaNum
        GWRxSignalfft(i,:) = fft(GWRxSignal(i,1:NumberFFT1),NumberFFT1);
        %     figure;plot(10*log10(abs(GWRxSignalfft(i,:)).^2));
    end
    GWRxAH = GWRxSignalfft(:,FreqPoint+1);
    
    %
    for i = 1:NumberFFT1
        t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %观测点
    end
    GWTxSignal0 = zeros(GWRxAntennaNum,NumberFFT1);
    for i=1:GWRxAntennaNum
        for j=1:NumberFFT1
            if j>=1024*(i-1)+1 && j<=1024*i
                GWTxSignal0(i,j) = cos(2*pi*TxFreq*t(j)) + sin(2*pi*TxFreq*t(j))*1i;
            end
        end
    end
    GWRxAH = GWRxAH./sqrt(sum(abs(GWRxAH).^2));
    %使用实际信道进行下行BF
    % GWRxAH = HTemp1.*GWAntennaRxPhaseNoReciprocal;
    Hhat(:,1) = GWRxAH(:,1)./(CalibrateFactor.');
    
    BF = Hhat';
    BFOutMax = max(abs(BF).^2);
    for i=1:GWRxAntennaNum
        TxAmpc(1,i) = sqrt(BFOutMax)/abs(BF(i)).^2;
        BF(i) = BF(i)*TxAmpc(i);
    end
    
    BFOut = BF;
    GWTxSignal1 = GWTxSignal0.*BFOut.';
    GWTxSignal2 = GWTxSignal0.*BF.';
    GWTxSignal = [GWTxSignal1 zeros(8,8192) GWTxSignal2];
    %射频发射支路引起信道不互易分量
    GWTxSignal = GWTxSignal.*GWAntennaTxPhaseNoReciprocal;
    
    Noise = (rand(1,length(GWTxSignal)) + (1j)*rand(1,length(GWTxSignal))) * sqrt(noiseVar/2);
    UserRxSignal = HTemp1.'*GWTxSignal + 0*Noise;
    % figure;plot(real(UserRxSignal))
    % CalibrateFactorOut = ones(1,8);
end