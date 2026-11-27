function [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactorOut_REG,GWRXSNR_REG,GWTXSNR_REG] = GWAntennaRecipracal(SimPara,GWRXPara)

% GWRxAntennaNum = SimPara.AntennaNum;
GWRxAntennaNum = GWRXPara.AntennaNum;

if GWRxAntennaNum==1
    GWAntennaRxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    GWAntennaTxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    CalibrateFactorOut_REG(1:SimPara.AntennaNum) = ones(1,GWRxAntennaNum);
    GWRXSNR_REG = 40;
    GWTXSNR_REG = 40;
else
    s = RandStream('mt19937ar','Seed',5);
    RandStream.setGlobalStream(s);
    
    SnrSamplingRate = 20;
    noiseVar = 1/10^(SnrSamplingRate/10);    % ÔëÉù·½²î£¬½ÓÊÕ¶ËÒÔ·ûºÅËÙÂÊÎª²ÉÑùËÙ¶È£¬·¢ËÍÐÅºÅ¹¦ÂÊÎª1£¬ËùÒÔÔëÉù·½²îÒ²ÊÇÔëÉù¹¦ÂÊ
    
    GWAntennaRxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    GWAntennaTxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i=1:GWRxAntennaNum
        AmpcRx(i) = 1+0.03*randi([-5 5],1,1);
        AmpcTx(i) = 1+0.03*randi([-5 5],1,1);
        GWAntennaRxPhaseNoReciprocal(i,1) = AmpcRx(i)*GWAntennaRxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaRxPhaseNoReciprocal(i,1).^2))); %¹éÒ»»¯µ±Ç°ÐÅºÅµÄÐÅµÀÏìÓ¦
        GWAntennaTxPhaseNoReciprocal(i,1) = AmpcTx(i)*GWAntennaTxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaTxPhaseNoReciprocal(i,1).^2))); %¹éÒ»»¯µ±Ç°ÐÅºÅµÄÐÅµÀÏìÓ¦
    end
    
    %RX TX NoRecipracal==1
    if SimPara.RecipracalCaliFlag==0
        GWAntennaRxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
        GWAntennaTxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
    end
    
    %% ACM·¢£¬DataÍ¨µÀÊÕ
    %ÐÅºÅÉú³É
    NumberFFT=512*(GWRXPara.GWRxSampleRate/125000); %FFT¸öÊý
    Freqdif = GWRXPara.GWRxSampleRate/NumberFFT;
    % FreqStartPos = 100;
    FreqStartPos = 10;
    NumDiff=1; % ÔØ²¨¼ä¸ôµãÊý Á½µã¼ä¸ô125000/1024
    NumCarrier=GWRxAntennaNum*2-1;  %number of carrier 2*7-1  %ºóÐø½«¼õÉÙµ½8¸ö test_slot0508_22 test_channel3_noise18
    offset = Freqdif*FreqStartPos;
    for i=1:NumCarrier
        Freq(i) =offset+Freqdif*i;
    end
    P=[-1.02324779750120,-2.58039305874270,-2.78523111484624,0.363193019902249,1.29378952938372,2.13681484724016,-1.00220928762122,0.207595752893231,-3.02187936892107,0.936349644728564,-2.36334565934660,0.126400964528446,-0.0828443127029830,1.02527778844203,0.371577395760742];
    % P = zeros(1,15);
    for i = 1:NumberFFT
        t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %¹Û²âµã
    end
    ACMTxSignal = zeros(1,NumberFFT);
    for i=1:NumCarrier
        Tone(i,:) = cos(2*pi*Freq(i)*t-P(i)) + sin(2*pi*Freq(i)*t-P(i))*1i;
        ACMTxSignal = ACMTxSignal + Tone(i,:); %Éè¶¨5¸ö¸´ÊýÕýÏÒ·¢ÉäÐÅºÅ
    end
    
    HTemp = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i = 1:1
        HTemp(:,i) = sqrt(GWRxAntennaNum)*HTemp(:,i)/sqrt(sum(abs(HTemp(:,i).^2))); %¹éÒ»»¯µ±Ç°ÐÅºÅµÄÐÅµÀÏìÓ¦
    end
    Noise = (rand(GWRxAntennaNum,length(ACMTxSignal)) + (1j)*rand(GWRxAntennaNum,length(ACMTxSignal))) * sqrt(noiseVar/2);
    GWRxDataChannelSingla = HTemp*ACMTxSignal + Noise;
    
    %ÉäÆµ½ÓÊÕÖ§Â·ÒýÆðÐÅµÀ²»»¥Ò×·ÖÁ¿
    GWRxDataChannelSingla = GWRxDataChannelSingla.*GWAntennaRxPhaseNoReciprocal;
    % GWRxDataChannelSingla = GWRxDataChannelSingla./GWAntennaRxPhaseNoReciprocal;
    %% DataÍ¨µÀ·¢£¬ACMÊÕ
    DataTxSignal = zeros(GWRxAntennaNum,NumberFFT);
    for i=1:NumCarrier
        Tone(i,:) = cos(2*pi*Freq(i)*t) + sin(2*pi*Freq(i)*t)*1i;
    end
    for i=1:GWRxAntennaNum
        if i==1
            DataTxSignal(i,:) = Tone(1,:);
            for j=1:(GWRxAntennaNum-1)
                DataTxSignal(i,:) = DataTxSignal(i,:) + Tone(j*2,:);
            end
        else
            DataTxSignal(i,:) = Tone(2*i-1,:);
        end
    end
    %ÉäÆµ·¢ÉäÖ§Â·ÒýÆðÐÅµÀ²»»¥Ò×·ÖÁ¿
    DataTxSignal = DataTxSignal.*GWAntennaTxPhaseNoReciprocal;
    % DataTxSignal = DataTxSignal./GWAntennaTxPhaseNoReciprocal;
    
    Noise = (rand(1,length(DataTxSignal)) + (1j)*rand(1,length(DataTxSignal))) * sqrt(noiseVar/2);
    GWRxACMChannelSignal = HTemp.'*DataTxSignal + Noise;
    
    %% Ð£×¼Òò×Ó¼ÆËã
    for i=1:GWRxAntennaNum
        GWRxDataChannelSinglafft(i,:) = fft(GWRxDataChannelSingla(i,1:NumberFFT),NumberFFT);
        %     figure;plot(10*log10(abs(GWRxDataChannelSinglafft(i,:)).^2));
        GWRXSNR_REG(i) = (min(20*log10(abs(GWRxDataChannelSinglafft(i,(FreqStartPos+2):(FreqStartPos+2)+(NumCarrier-1)*NumDiff))))) ...
        - (10*log10((abs(GWRxDataChannelSinglafft(i,(FreqStartPos+1)))^2 + abs(GWRxDataChannelSinglafft(i,(FreqStartPos+3)+(NumCarrier-1)*NumDiff))^2)/2));
    end
    
    GWRxACMChannelSignalfft = fft(GWRxACMChannelSignal(1,1:NumberFFT),NumberFFT);
    % figure;plot(10*log10(abs(GWRxACMChannelSignalfft(1,:)).^2));
    ANoise=(10*log10((abs(GWRxACMChannelSignalfft(1,(FreqStartPos+1)))^2 + abs(GWRxACMChannelSignalfft(1,(FreqStartPos+3)+(NumCarrier-1)*NumDiff))^2)/2));
    for i=1:2*NumDiff:NumCarrier
        GWTXSNR_REG((i+1)/2) = 20*log10(abs(GWRxACMChannelSignalfft(1,FreqStartPos+1+i))) - ANoise;
    end
    
    Cphase1=GWRxDataChannelSinglafft(:,(FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);
    UplinkC=zeros(1,NumCarrier);
    UplinkC(1,1:2)=Cphase1(1,1:2);
    UplinkC(1,4:2:end)=Cphase1(1,4:2:end);
    for j=3:2:NumCarrier
        UplinkC(1,j)=Cphase1((j+1)/2,j);
    end
    
    DownlnkC= GWRxACMChannelSignalfft((FreqStartPos+2):NumDiff:(FreqStartPos+2)+(NumCarrier-1)*NumDiff);
    
    UplinkC = UplinkC.*(cos(P(1:NumCarrier)) + sin(P(1:NumCarrier))*1i);
    Cphase2(1,:)=(UplinkC(1,:)./DownlnkC(1,:));
    % Cphase2(1,:)=(UplinkC(1,:)./conj(DownlnkC(1,:)));
    for j=3:2:NumCarrier
        temp2((j-1)/2) = (Cphase2(1,2)/Cphase2(1,1));
        temp1((j-1)/2) = Cphase2(1,j)/Cphase2(1,j-1);
        temp=(Cphase2(1,j)/Cphase2(1,1))/(Cphase2(1,j-1)/(Cphase2(1,2)));
        CalibrateFactor(1,(j-1)/2)=temp;
    end
    % for i=1:8
    %         %Test
    %     CalibrateFactorTemp(i) = Cphase2(1,2*i-1);
    % end
    % CalibrateFactorOut = CalibrateFactorTemp;
    
    %% SimÐ£×¼²»×¼È·ÐÔ
    temp = 0.01*SimPara.CalibrateFactorDiff;
    CalibrateFactorDiff = (temp*randi([-5 5],1,GWRxAntennaNum-1) + temp*randi([-5 5],1,GWRxAntennaNum-1)*1i);
    x1=abs(CalibrateFactorDiff);
    y1=angle(CalibrateFactorDiff);
    x2=abs(CalibrateFactor);
    y2=angle(CalibrateFactor);
    
    % CalibrateFactor = CalibrateFactor + CalibrateFactorDiff;
    x3=abs(CalibrateFactor);
    y3=angle(CalibrateFactor);
    PhaseDiff = sum(abs(y3-y2))
    AmpcDiff = sum(abs(x3-x2))
    
    CalibrateFactorOut_REG(1) = 1;
    CalibrateFactorOut_REG(2:GWRxAntennaNum) = CalibrateFactor;
    
    K = dec2bin(GWRXPara.AntennaWorkStatus,8);
    for i=1:GWRxAntennaNum
        if K(GWRxAntennaNum-i+1)==dec2bin(1)

        else
            CalibrateFactorOut_REG(i) = 0;
        end
    end
    
    %У׼������֤
    [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactorOut_REG,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal);
end
end

function [UserRxSignal] = Test_CalibrateFactor(GWRXPara,CalibrateFactor,GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal)

    % % Dsp Êý¾Ýµ¼Èë
    % close all;
    % GWRxAntennaNum = 8;
    % NumberFFT = 512;
    % NumCarrier  = 15;
    % NumDiff = 1;
    %
    % file_name=['H:\Íø¹Ø×´Ì¬ÐÅÏ¢ÏÔÊ¾ÉÏÎ»»ú\data\data' num2str(1)];
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
   
    %% Test CalibrateFactorOut
    GWRxAntennaNum = GWRXPara.AntennaNum;
    SnrSamplingRate = 20;
    noiseVar = 1/10^(SnrSamplingRate/10);    % ÔëÉù·½²î£¬½ÓÊÕ¶ËÒÔ·ûºÅËÙÂÊÎª²ÉÑùËÙ¶È£¬·¢ËÍÐÅºÅ¹¦ÂÊÎª1£¬ËùÒÔÔëÉù·½²îÒ²ÊÇÔëÉù¹¦ÂÊ
    NumberFFT1 = 8192;
    FreqPoint = 500;
    TxFreq = FreqPoint*GWRXPara.GWRxSampleRate/NumberFFT1;
    for i = 1:NumberFFT1
        t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %¹Û²âµã
    end
    UserTxSignal = zeros(1,NumberFFT1);
    UserTxSignal = cos(2*pi*TxFreq*t) + sin(2*pi*TxFreq*t)*1i;
    
    HTemp1 = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i = 1:1
        HTemp1(:,i) = sqrt(GWRxAntennaNum)*HTemp1(:,i)/sqrt(sum(abs(HTemp1(:,i).^2))); %¹éÒ»»¯µ±Ç°ÐÅºÅµÄÐÅµÀÏìÓ¦
    end
    Noise = (rand(GWRxAntennaNum,length(UserTxSignal)) + (1j)*rand(GWRxAntennaNum,length(UserTxSignal))) * sqrt(noiseVar/2);
    GWRxSignal = HTemp1*UserTxSignal + Noise;
    %ÉäÆµ½ÓÊÕÖ§Â·ÒýÆðÐÅµÀ²»»¥Ò×·ÖÁ¿
    GWRxSignal = GWRxSignal.*GWAntennaRxPhaseNoReciprocal;
    for i=1:GWRxAntennaNum
        GWRxSignalfft(i,:) = fft(GWRxSignal(i,1:NumberFFT1),NumberFFT1);
        %     figure;plot(10*log10(abs(GWRxSignalfft(i,:)).^2));
    end
    GWRxAH = GWRxSignalfft(:,FreqPoint+1);
    
    %
    for i = 1:NumberFFT1
        t(i) = 1/GWRXPara.GWRxSampleRate * (i - 1); %¹Û²âµã
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
    %Ê¹ÓÃÊµ¼ÊÐÅµÀ½øÐÐÏÂÐÐBF
    % GWRxAH = HTemp1.*GWAntennaRxPhaseNoReciprocal;
    
    BF = GWRxAH';
    BFOutMax = max(abs(BF).^2);
    for i=1:GWRxAntennaNum
        TxAmpc(1,i) = sqrt(BFOutMax)/abs(BF(i)).^2;
        BF(i) = BF(i)*TxAmpc(i);
    end
    
    BFOut = BF.*CalibrateFactor;
    GWTxSignal = GWTxSignal0.*BFOut.';
    
    %ÉäÆµ·¢ÉäÖ§Â·ÒýÆðÐÅµÀ²»»¥Ò×·ÖÁ¿
    GWTxSignal = GWTxSignal.*GWAntennaTxPhaseNoReciprocal;
    
    Noise = (rand(1,length(GWTxSignal)) + (1j)*rand(1,length(GWTxSignal))) * sqrt(noiseVar/2);
    UserRxSignal = HTemp1.'*GWTxSignal + 0*Noise;
    % figure;plot(real(UserRxSignal))
    % CalibrateFactorOut = ones(1,8);
end