function signalArr = channelize20180408A( signalReceive, freqOffsetCh, overlapping, winLen, filterLen, sampleFrequency )
%   function：提取signalReceive的频域信号

%   signalReceive：传递进来的信号，行为天线数数，列为采样数
%   freqOffsetCh：每路信号的随机频偏，不同天线的同一个用户的频偏相同
%   overlapping：重叠长度
%   winLen：窗口长度
%   filterLen：滤波器长度，需要为偶数
%   sampleFrequency：采样率

%   signalArr：恢复出的频域信号，结构为numCh行，filterLen*signalRow*slideTime列
%       halfBL在参数设置中，slideTime由信号长度决定

[signalRow, signalCol] = size(signalReceive);
numCh = length(freqOffsetCh);
halfBL = filterLen/2;

%New HJH ADD 20220627
htmp = [-0.0009765625,-0.0029296875,-0.0029296875,0.0009765625,0.0048828125,0.00390625,-0.001953125,-0.0068359375,-0.005859375,0.0009765625,0.0078125,0.0087890625,0.001953125,-0.0078125,-0.009765625,-0.0009765625,0.0107421875,0.0126953125,-0.001953125,-0.0234375,-0.0302734375,-0.005859375,0.041015625,0.07421875,0.0517578125,-0.03515625,-0.1416015625,-0.1796875,-0.0693359375,0.2060546875,0.568359375,0.8779296875,1,0.8779296875,0.568359375,0.2060546875,-0.0693359375,-0.1796875,-0.1416015625,-0.03515625,0.0517578125,0.07421875,0.041015625,-0.005859375,-0.0302734375,-0.0234375,-0.001953125,0.0126953125,0.0107421875,-0.0009765625,-0.009765625,-0.0078125,0.001953125,0.0087890625,0.0078125,0.0009765625,-0.005859375,-0.0068359375,-0.001953125,0.00390625,0.0048828125,0.0009765625,-0.0029296875,-0.0029296875,-0.0009765625];
htmp = [ 0 htmp];
hTmp1 = upsample(htmp,2); % 2: 128kHz/16kHz*4
HCE = 9;
h = [zeros(1,winLen/2-length(hTmp1)/2) hTmp1 zeros(1,winLen/2-length(hTmp1)/2)];
HC = conj(fft(h)/sqrt(winLen)); % figure; plot(semilogy(abs(HC).^2));
HC = [HC(end-halfBL+1:end) HC(1:halfBL)]; % figure; plot(semilogy(abs(HC).^2));
HC = HC*HCE;

% 处理signalReceive的长度使最后一次滑动滤波后无剩余数据
slideTime = ceil((signalCol)/(winLen)); % 滑动次数



% 转换频偏为整数频偏
freqOffsetChGrid = round((freqOffsetCh+sampleFrequency)/sampleFrequency*winLen);

% fft
signalArr = zeros(numCh, filterLen*signalRow*slideTime);
for j = 1:slideTime
    % 截取一段信号
    signalFragment = signalReceive(:,(j-1)*winLen+1:j*winLen);
    % fft处理矩阵时按列进行，所以先对行信号转置，达到对行做fft的目的
    signalFft = fft(transpose(signalFragment))/winLen;
    for i = 1:numCh
        % 将第i路信号循环移位到零频
        signal1 = circshift(signalFft,-freqOffsetChGrid(i));
        % 取出第i路的信号，按顺序排列多根天线的数据为一行，一根天线的数据结束之后排列第二根天线的数据
        signal2 = reshape([signal1(end-halfBL+1:end,:); signal1(1:halfBL,:)],1,[]);
        signalArr(i,(j-1)*filterLen*signalRow+1:j*filterLen*signalRow) = signal2.*HC;
    end
end

end






