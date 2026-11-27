function [ signalArr ] = channelize20180408C( signalReceive, freqOffsetCh, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, realLen, bool )
%	function：对signalReceive滤波，恢复numCh路信号返回

%   signalReceive：传递进来的经过多天线合并后的频域信号
%   filterLen：一路信号一次滑动滤波需要参与计算的点数
%   freqOffsetCh：每路信号的频偏
%   realLen：原始信号的长度，在A部分对信号尾部添加了零，最终输出时要把零去掉，需要realLen这个信息

%   signalArr：恢复出的numCh路信号


halfBL = filterLen/2;    % 滤波器的长度的二分之一
BlkLen = (winLen-overlapping)/(fs/fs2); % 一次滤波输出长度
BlkLen1 = BlkLen + 8;
[numCh, signalLen] = size(signalReceive);

% 生成匹配滤波器
h = [zeros(1,winLen/2-fs) sin(pi*(1/(4*fs):1/(2*fs):(4*fs-1)/(4*fs))) zeros(1,winLen/2-fs)];
h(end/2+1:end) = fliplr(h(1:end/2));
h = [zeros(1,winLen/2-2*fs) geth(fs) zeros(1,winLen/2-2*fs)];
HC = conj(fft(h)/sqrt(winLen));

if bool == 0
    HC = ones(1,filterLen);
else
    HC = [HC(end-halfBL+1:end) HC(1:halfBL)] * sqrt(6.6);
end

% 求滑动次数，由于函数A已经处理了长度，所以这里slideTime为整数
slideTime = signalLen/filterLen;

% 转换频偏为整数频偏
freqOffsetChGrid = round((freqOffsetCh+sampleFrequency)/sampleFrequency*winLen);

% 匹配滤波
signalArr = zeros(numCh, (slideTime-2)*BlkLen+2*BlkLen1);
for j = 1:slideTime
    % 取出一段数据
    signalFft = signalReceive(:,(j-1)*filterLen+1:j*filterLen);
    for i = 1:numCh
        signal1 = signalFft(i,:) .* HC;
        % sum(abs(signalFft(i,:)).^2)/sum(abs(signal1).^2)
        signal2 = [signal1(halfBL+1:end) zeros(1,2048-halfBL*2) signal1(1:halfBL)]*exp((1i)*2*pi*freqOffsetChGrid(i)*(j-1)*overlapping/winLen);
        signal3 = ifft(signal2,2048)*sqrt(2048);
        if j == 1
            signal4 = [signal3(1025:end) signal3(1:1016)];
            signalArr(i,1:BlkLen1) = signal4;
        elseif j == slideTime
            signal4 = [signal3(1033:end) signal3(1:1024)];
            signalArr(i,(j-2)*BlkLen+BlkLen1+1:(j-2)*BlkLen+BlkLen1+BlkLen1) = signal4;
        else
            signal4 = [signal3(1033:end) signal3(1:1016)];
            signalArr(i,(j-2)*BlkLen+BlkLen1+1:(j-1)*BlkLen+BlkLen1) = signal4;
        end
    end
end

signalArr = signalArr(:,1:realLen/(fs/fs2));

end

