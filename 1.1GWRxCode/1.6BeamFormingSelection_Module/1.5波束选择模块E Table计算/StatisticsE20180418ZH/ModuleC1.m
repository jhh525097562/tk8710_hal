
clear all
close all

% 首先生成发射信号
% 然后使用A模块计算信道增益
% 最后选择序列

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
Plen = 256*fs;

frameNumber = 600;          % 帧数
M = 4;                      % 用户数量
N = 8;                      % 天线数量
SNR = 20;
supressNum = 7;             % 抑制分量个数

PCoef = -99;                  % 功率差
HCoef = sqrt(0.1);          % 信道估计误差
alpha = 0.9;                % 恒模权重

errSelect = zeros(length(SNR),1);
errAll = zeros(length(SNR),1);
errMMSE = zeros(length(SNR),1);

load('SNRTable.mat');

% 生成前后两段信号
h = geth(fs);

% 不同的帧
for ii = 1:frameNumber
        
    % 不同的噪声功率
    for jj = 1:length(SNR)

        % 生成符号
        symbols = 2*randi([0,1],LS,M)-1;
        % 生成MSK信号
        modSignal = zeros(LS*fs,M);
        info = symbols;
        info(2:2:end,:) = info(2:2:end,:)*(1i);
        infoup = upsample(info,fs);
        for ll = 1:M
            temp1 = conv(infoup(:,ll),h);
            modSignal(:,ll) = temp1(2*fs+1:end-2*fs+1);
        end
        MSK = transpose(modSignal);
        % 生成导频
        preamble = exp((1i)*2*pi*976.6*(0:(Plen-1))/sampleFrequency);
        % 生成发送信号
        MSignal = [MSK(:,1:(frameLen-Plen)/2) repmat(preamble,M,1) MSK(:,(frameLen+Plen)/2+1:end)];
        % plot(real(MSignal(1,:)));
        % 随机生成频偏
        freq = randi([-sampleFrequency/2 sampleFrequency/2],1,M);
        freqInt = round(freq/sampleFrequency*winLen)/winLen*sampleFrequency;
        % 随机频偏信号
        Tx = MSignal.*exp((1i)*2*pi*freqInt'*(0:frameLen-1)/sampleFrequency);

        % 生成干扰功率
        if PCoef == 1
            D = eye(M);
        else
            D = diag(10.^((PCoef+20*rand())/20));
            D(1,1) = 1;
        end

        % 生成噪声
        sigma = sqrt(1/10^(SNR(jj)/10));
        noise = sigma*(randn(N,frameLen)+(1i)*randn(N,frameLen))/sqrt(2);

        % 生成信道矩阵
        H = (randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N);
        % 期望模块A能够估计出的信道矩阵
        Hhat = H*D;

        % 生成接收信号
        Rx = H*D*Tx + noise;

        % 模块A估计信道增益-----------------
        % 截取导频
        Pre = Rx(:,(frameLen-Plen)/2+1:(frameLen+Plen)/2);
        % 加窗
        Win = sin(pi*(0:winLen-1)/(winLen-1));
        PreWin = Pre.*repmat(Win,N,1);

        % PreWinNorm = diag(mean(abs(Pre),2)./mean(abs(PreWin),2))*PreWin;
        PreWinNorm = PreWin;

        % fft
        % PreFft = transpose(fft(transpose(PreWinNorm))/sqrt(winLen));
        PreFft = transpose(fft(transpose(PreWinNorm)));
        % plot(abs(PreFft(1,:)));
        % 寻找导频
        freqBin = round((freqInt+976.6)/sampleFrequency*winLen);
        freqBin(freqBin>0) = freqBin(freqBin>0) + 1;
        freqBin(freqBin<0) = winLen+freqBin(freqBin<0) + 1;
        freqBin(freqBin==0) = 1;
        % 信道估计
        % AH = PreFft(:,freqBin)/sqrt(winLen);
        
        AH = diag(mean(abs(Pre),2)./mean(abs(PreWin),2))*PreFft(:,freqBin)/winLen;

        % 匹配滤波
        dataRec1 = channelize20180408A(Rx, freqInt, overlapping, winLen, filterLen, sampleFrequency);
        dataRec2 = channelize20180408B(dataRec1, AH, filterLen, N, supressNum);
        dataRec3 = channelize20180408C(dataRec2, reshape(repmat(freqInt,(supressNum+1),1),[],1), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1);

        % 恒模和功率选择算法
        dataRec4 = dataRec3(1:N,:)/1.8162;

        % 求E
        dataAbs = abs(dataRec4);
        P = dataAbs*dataAbs'/length(dataAbs);
        PP = diag(sqrt(P));
        dataNormal = diag(1./PP)*dataAbs;
        E = mean(dataNormal,2);

        % 期望功率
        powExp = sum(abs(AH(:,1)).^2);
        powTemp = sum(abs(Hhat(:,1)).^2);
        powExp/powTemp
        % 查SNR
        select = zeros(N,1);
        for ll = 1:N
            diff = abs(E(ll)-SNRTable(:,3));
            [tempmin,index] = min(diff);
            SNRSelect1 = SNRTable(index,2);
            Pmsk = P(ll,ll)*10^(SNRSelect1/10)/(10^(SNRSelect1/10)+1);
            select(ll) = alpha*(SNRSelect1) - (1-alpha)*abs(10*log10(Pmsk/powExp)+3)^2;
        end

        [~,index] = max(select);
        dataRec5 = dataRec4(index,:);

        % 求相关，进而求相差
        seq1 = MSignal(1,fs+1:fs:end-fs);
        seq2 = dataRec5(1,fs2+1:fs2:end-fs2);
        R = corrcoef(seq1,seq2);
        dataRec = dataRec5/R(1,2);
        % plot(real(dataRec));

        % 计算误码率
        [~, errorRate] = calaulateError(dataRec, MSignal(1,:));
        errSelect(jj) = errSelect(jj) + errorRate/frameNumber;
        
        
        % 与2^7比较
        dataRec11 = channelize20180408A(Rx, freqInt, overlapping, winLen, filterLen, sampleFrequency);
        dataRec12 = compareBest(dataRec11(1,:), filterLen, N, Hhat);
        dataRec13 = channelize20180408C(dataRec12, repmat(freqInt(1),2^supressNum,1), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1);
        [~, errorRate2] = calaulateError(dataRec13, repmat(MSignal(1,:),2^supressNum,1));
        errorMin = min(errorRate2);
        errAll(jj) = errAll(jj) + errorMin/frameNumber;

        % 与MMSE比较
        RxMMSE = (Hhat'*Hhat+sigma^2*10^(-1.505)*eye(M))\Hhat'*Rx;
        dataRec21 = channelize20180408A(RxMMSE(1,:), freqInt(1), overlapping, winLen, filterLen, sampleFrequency);
        dataRec22 = channelize20180408C(dataRec21, freqInt(1), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1)/2;
        [~, errorRate3] = calaulateError(dataRec22, MSignal(1,:));
        errMMSE(jj) = errMMSE(jj) + errorRate3/frameNumber;
        
    end
    if mod(ii,20) == 0
        fprintf('frame numebr: %d\n',ii);
    end
end

figure
semilogy(SNR,errSelect,'-*b',SNR,errAll,'-or',SNR,errMMSE,'-*r');
legend('algrithm','2^7','MMSE');
grid on










    
    
