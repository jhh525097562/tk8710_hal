
clear all
close all

% 首先生成发射信号，设置噪声极小
% 然后使用A模块计算信道增益
% 记录滤波后MSK信号的功率与滤波后导频信号的功率之比，多次仿真求平均值

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

frameNumber = 1000;          % 帧数
M = 4;                      % 用户数量
N = 8;                      % 天线数量
SNR = 99;
supressNum = 7;             % 抑制分量个数

PCoef = -99;                  % 功率差
HCoef = 0;          % 信道估计误差

RSeg = zeros(3,frameNumber);
RSave = zeros(2,frameNumber);

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

        % 发送信号
        Tx = MSignal;

        % 生成干扰功率
        if PCoef == 1
            D = eye(M);
        else
            D = diag(10.^((PCoef+20*rand(1,M))/20));
            D(1,1) = 1;
        end

        % 生成噪声
        sigma = sqrt(1/10^(SNR(jj)/10));
        noise = sigma*(randn(N,frameLen)+(1i)*randn(N,frameLen))/sqrt(2);

        % 生成信道矩阵
        H = (randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N);

        % 生成接收信号
        Rx = H*D*Tx + noise;

        % 匹配滤波
        dataRec1 = channelize20180408A(Rx, zeros(1,M), overlapping, winLen, filterLen, sampleFrequency);
        dataRec2 = channelize20180408B(dataRec1, H*D, filterLen, N, supressNum);
        dataRec3 = channelize20180408C(dataRec2, reshape(repmat(zeros(1,M),(supressNum+1),1),[],1), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1);

        
        % 统计功率
        data = dataRec3(1,9:end-8);
        plot(abs(data));
        
        data_seg = reshape(data,[],3);
        
        seg_abs = abs(data_seg);
        
        RSeg(:,ii) = mean(seg_abs.^2);
        
        RSave(1,ii) = RSeg(1,ii) / RSeg(2,ii);
        RSave(2,ii) = RSeg(3,ii) / RSeg(2,ii);
        
    end
    if mod(ii,100) == 0
        fprintf('frame numebr: %d\n',ii);
    end
end

mean(RSave,2)

% 1.1681









    
    
