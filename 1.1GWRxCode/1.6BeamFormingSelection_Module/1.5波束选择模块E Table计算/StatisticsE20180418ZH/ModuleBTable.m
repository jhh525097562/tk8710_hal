
clear all
close all

tic

% 参数设置
fs = 32;                    % 滤波前符号周期
fs2 = 8;                    % 滤波后符号周期
sampleFrequency = 125000;   % 采样率
winLen = 8192;              % 窗口长度
overlapping = 64;           % 重叠长度
filterLen = 256;            % 滤波器长度

segment = 3;                % 滑动次数
frameLen = winLen*segment - (segment-1)*overlapping;    % 一帧的采样点数
LS = ceil(frameLen/fs);     % 仿真码元个数

frameNumber = 100;          % 帧数
SNR = [10];             % 信噪比

% 滤波器
h = geth(fs);

SNRCheck = zeros(frameNumber,length(SNR));
ECount = zeros(frameNumber,length(SNR));
SNRTable = zeros(length(SNR),3);
SNRTable(:,1) = SNR';

for ii = 1:frameNumber
    
    % 生成符号
    symbols = randi([0 1],LS,1);
    symbols(symbols==0) = -1;
    info = symbols;
    info(2:2:end,:) = info(2:2:end,:)*(1i);
    infoup = upsample(info,fs);
    
    temp = conv(infoup,h);
    modSignal = temp(2*fs:end-2*fs);
    MSK = transpose(modSignal(:));
    
    % MSK信号滤波
    MSK1 = channelize20180408A(MSK, 0, overlapping, winLen, filterLen, sampleFrequency);
    MSK2 = channelize20180408C(MSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2;
    
    figure;
    plot(abs(MSK2));
    
    MSKPower = MSK2*MSK2';
    
    for jj = 1:length(SNR)
        
        % 生成噪声
        sigma = sqrt(1/(10^(SNR(jj)/10)));
        noise = sigma*(randn(1,length(MSK))+(1i)*randn(1,length(MSK)))/sqrt(2);
        
        % 噪声滤波
        noise1 = channelize20180408A(noise, 0, overlapping, winLen, filterLen, sampleFrequency);
        noise2 = channelize20180408C(noise1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
        noisePower = noise2*noise2';
        
        SNRHat = 10*log10(MSKPower/noisePower);
        SNRCheck(ii,jj) = SNRHat;
        
%         % 检测滤波前相加和滤波后相加是否相同，相同
%         dataA = MSK + noise;
%         dataA1 = channelize20180408A(dataA, 0, overlapping, winLen, filterLen, sampleFrequency);
%         dataA2 = channelize20180408C(dataA1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
%         dataB2 = MSK2 + noise2;
%         plot(real(dataA2-dataB2));
        
        % 统计
        data = MSK2 + noise2;
        dataAbs = abs(data);
        P = dataAbs*dataAbs'/length(dataAbs);
        dataNormal = dataAbs/sqrt(P);
        E = mean(dataNormal);
        ECount(ii,jj) = E;
 
    end

end

SNRTable(:,2) = mean(SNRCheck);
SNRTable(:,3) = mean(ECount)

save('SNRTable.mat','SNRTable');


toc



















