function [GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,CalibrateFactorOut] = GWAntennaRecipracalNew(SimPara,GWRXPara)

% clear all;
% close all;
% SysSimulationPara.AntennaNum = 8;
% SysSimulationPara.GWRxSampleRate = 125000;
% SysSimulationPara.CalibrateFactorDiff = 0;

GWRxAntennaNum = SimPara.AntennaNum;

if GWRxAntennaNum==1
    GWAntennaRxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    GWAntennaTxPhaseNoReciprocal(1:GWRxAntennaNum) = ones(1,GWRxAntennaNum);
    CalibrateFactorOut_REG(1:SimPara.AntennaNum) = ones(1,GWRxAntennaNum);
else
    GWAntennaRxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    GWAntennaTxPhaseNoReciprocal = (randn(GWRxAntennaNum,1) + 1i* randn(GWRxAntennaNum,1));
    for i=1:GWRxAntennaNum
        AmpcRx(i) = 1+0.03*randi([-5 5],1,1);
        AmpcTx(i) = 1+0.03*randi([-5 5],1,1);
        GWAntennaRxPhaseNoReciprocal(i,1) = AmpcRx(i)*GWAntennaRxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaRxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
        GWAntennaTxPhaseNoReciprocal(i,1) = AmpcTx(i)*GWAntennaTxPhaseNoReciprocal(i,1)/sqrt(sum(abs(GWAntennaTxPhaseNoReciprocal(i,1).^2))); %归一化当前信号的信道响应
    end
    %test
    % GWAntennaRxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
    % GWAntennaTxPhaseNoReciprocal = ones(GWRxAntennaNum,1);
    
    %以通道2为ACM，通道1为参考通道，得到3,4,5,6,7,8校准因子
    SelectAntenna = 2;%选择作为校准的天线
    CalibrateFactor1 = TestAntennaRecipracal(GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,SelectAntenna,GWRxAntennaNum,GWRXPara.GWRxSampleRate);
    %以通道3为ACM，通道1为参考通道，得到2,4,5,6,7,8校准因子
    SelectAntenna = 3;%选择作为校准的天线
    CalibrateFactor2 = TestAntennaRecipracal(GWAntennaRxPhaseNoReciprocal,GWAntennaTxPhaseNoReciprocal,SelectAntenna,GWRxAntennaNum,GWRXPara.GWRxSampleRate);
    CalibrateFactor = [CalibrateFactor2(1) CalibrateFactor1];
    
    
    %校准不准确性
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
    
    CalibrateFactorOut(1) = 1;
    CalibrateFactorOut(2:GWRxAntennaNum) = CalibrateFactor;
    %% Test CalibrateFactorOut
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
    
    BF = GWRxAH';
    BFOutMax = max(abs(BF).^2);
    for i=1:GWRxAntennaNum
        TxAmpc(1,i) = sqrt(BFOutMax)/abs(BF(i)).^2;
        BF(i) = BF(i)*TxAmpc(i);
    end
    
    BFOut = BF.*CalibrateFactorOut;
    GWTxSignal = GWTxSignal0.*BFOut.';
    
    %射频发射支路引起信道不互易分量
    GWTxSignal = GWTxSignal.*GWAntennaTxPhaseNoReciprocal;
    
    Noise = (rand(1,length(GWTxSignal)) + (1j)*rand(1,length(GWTxSignal))) * sqrt(noiseVar/2);
    UserRxSignal = HTemp1.'*GWTxSignal + 0*Noise;
    
    % figure;plot(real(UserRxSignal))
    % CalibrateFactorOut = ones(1,8);
    % end
end