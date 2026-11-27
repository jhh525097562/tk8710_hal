
clear all
close all

tic

% 参数设置
fs = 8;                    % 滤波前符号周期
fs2 = 4;                    % 滤波后符号周期
sampleFrequency = 125000;   % 采样率
winLen = 8192;              % 窗口长度
overlapping = 128;           % 重叠长度
filterLen = 1024;            % 滤波器长度

segment = 3;                % 滑动次数
frameLen = winLen*segment - (segment-1)*overlapping;    % 一帧的采样点数
LS = ceil(frameLen/fs);     % 仿真码元个数

frameNumber = 100;          % 帧数
SNR = [-25:1:24];             % 信噪比

% 滤波器
% h = geth(fs);
% BaseUpsamplingRateTx = 8;
% FilterRRCTx = zeros(1,6*BaseUpsamplingRateTx-1);
% tmpF = rcosdesign(0.485,6,BaseUpsamplingRateTx,'sqrt');
% figure;
% plot(tmpF);
% FilterRRCTx = tmpF(1,2:end-1)./(max(tmpF));

BaseUpsamplingRateTx = 8;
FilterSym = 6;
FilterRRCTxTmp = zeros(1,FilterSym*BaseUpsamplingRateTx);
tmpF = rcosdesign(0.485,FilterSym,BaseUpsamplingRateTx,'sqrt');
% FilterRRCTxTmp = tmpF(1,2:end-1)./(max(tmpF));
FilterRRCTxTmp = tmpF(1,1:end)./(max(tmpF));

%     FilterRRCTxTmp1 = (FilterRRCTxTmp(1,floor(length(FilterRRCTxTmp)/2))*2 + FilterRRCTxTmp(1,ceil(length(FilterRRCTxTmp)/2)))/3;
FilterRRCTxTmp1 = 1;
FilterRRCTxTmp2 = [FilterRRCTxTmp(1,1:floor(length(FilterRRCTxTmp)/2)) FilterRRCTxTmp1];
FilterRRCTxTmp3 = [FilterRRCTxTmp2 fliplr(FilterRRCTxTmp2)];

h = FilterRRCTxTmp3;
figure;
plot(h);

SNRCheck = zeros(frameNumber,length(SNR));
ECount = zeros(frameNumber,length(SNR));
SNRTable = zeros(length(SNR),3);
SNRTable(:,1) = SNR';

for ii = 1:frameNumber
    
    % 生成符号
%     symbols = randi([0 1],LS,1);
%     symbols(symbols==0) = -1;
%     info = symbols;
%     info(2:2:end,:) = info(2:2:end,:)*(1i);

    ModOrder = 3;
    symbolsbit = randi([0 1],1,LS*ModOrder);

    tmpM = zeros(1,LS);
    for i1=1:ModOrder
        tmpM = tmpM + 2^(ModOrder-i1)*symbolsbit(1,i1:ModOrder:end);
    end
    info(1,:) = pskmod(tmpM,2^ModOrder,0,'gray');
    
    infoup = upsample(info,fs);
    
    temp = conv(infoup,h);
    modSignal = temp(ceil(length(h)/2):end-floor(length(h)/2));
    PSK = transpose(modSignal(:));
    
%     figure;
%     plot(abs(PSK));
    
    % MSK信号滤波
    PSK1 = channelize20180408A(PSK, 0, overlapping, winLen, filterLen, sampleFrequency);
    PSK2 = channelize20180408C(PSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2;
%     PSK2 = channelize20180408C(PSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1);
%     figure;
%     plot(abs(PSK2));
    
    PSKPower = PSK2*PSK2';
    
    for jj = 1:length(SNR)
        
        % 生成噪声
        sigma = sqrt(1/(10^(SNR(jj)/10)));
        noise = sigma*(randn(1,length(PSK))+(1i)*randn(1,length(PSK)))/sqrt(2);
        
        % 噪声滤波
        noise1 = channelize20180408A(noise, 0, overlapping, winLen, filterLen, sampleFrequency);
        noise2 = channelize20180408C(noise1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
        noisePower = noise2*noise2';
        
        SNRHat = 10*log10(PSKPower/noisePower);
        SNRCheck(ii,jj) = SNRHat;
        
%         % 检测滤波前相加和滤波后相加是否相同，相同
%         dataA = MSK + noise;
%         dataA1 = channelize20180408A(dataA, 0, overlapping, winLen, filterLen, sampleFrequency);
%         dataA2 = channelize20180408C(dataA1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
%         dataB2 = MSK2 + noise2;
%         plot(real(dataA2-dataB2));
        
        % 统计
        data = PSK2 + noise2;
        dataAbs = abs(data);
        P = dataAbs*dataAbs'/length(dataAbs);
        dataNormal = dataAbs/sqrt(P);
        dataAbs0 = abs(data).^2;
        p0 = sum(dataAbs0)/length(dataAbs0);
        dataNormal0 = (dataAbs0./p0).^2;
        E = mean(dataNormal);
        E0 = mean(dataNormal0);
        ECount(ii,jj) = E0;
 
    end

end

SNRTable(:,2) = mean(SNRCheck);
SNRTable(:,3) = mean(ECount)
PSKTable = SNRTable;
save('PSKTable20221019.mat','PSKTable');


toc



















