function [ signalArr ] = channelize20180408C( signalReceive, freqOffsetCh, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, realLen, bool )
%	function：对signalReceive滤波，恢复numCh路信号返回

%   signalReceive：传递进来的经过多天线合并后的频域信号
%   filterLen：一路信号一次滑动滤波需要参与计算的点数
%   freqOffsetCh：每路信号的频偏
%   realLen：原始信号的长度，在A部分对信号尾部添加了零，最终输出时要把零去掉，需要realLen这个信息

%   signalArr：恢复出的numCh路信号


halfBL = filterLen/2;    % 滤波器的长度的二分之一
BlkLen = (winLen-overlapping)/(fs/fs2); % 一次滤波输出长度
margin = overlapping/2/(fs/fs2);                % ifft滤波，头和尾会消失，消失的长度
BlkLen1 = BlkLen + margin;
ifftLen = winLen/(fs/fs2);                      % ifft的长度
leftEnd = round(BlkLen/2);                      % 截取1：leftEnd作为输出
rightStart = leftEnd+margin*2+1;                % 截取rightStart：end作为输出
[numCh, signalLen] = size(signalReceive);

% 生成匹配滤波器
% h = [zeros(1,winLen/2-fs) sin(pi*(1/(4*fs):1/(2*fs):(4*fs-1)/(4*fs))) zeros(1,winLen/2-fs)];
% h(end/2+1:end) = fliplr(h(1:end/2));
% h = [zeros(1,winLen/2-2*fs) geth(fs) zeros(1,winLen/2-2*fs)];

FilterRRCTxTmp = [-0.00809674359268633,-0.0155781375617193,-0.0164264182467650,-0.0110680073489793,0.00212489769089282,0.0177559742579431,0.0326473576565597,0.0395653787873241,0.0326751749761926,0.0111559094213285,-0.0272327101941122,-0.0715888388774971,-0.115956507852977,-0.141971099105005,-0.138467747216662,-0.0922947191870237,0.00276390249946876,0.143004420875350,0.320327399511067,0.513865825597022,0.701834251755972,0.859105199234349,0.962610475258520,1,0.962610475258520,0.859105199234349,0.701834251755972,0.513865825597022,0.320327399511067,0.143004420875350,0.00276390249946876,-0.0922947191870237,-0.138467747216662,-0.141971099105005,-0.115956507852977,-0.0715888388774971,-0.0272327101941122,0.0111559094213285,0.0326751749761926,0.0395653787873241,0.0326473576565596,0.0177559742579431,0.00212489769089282,-0.0110680073489793,-0.0164264182467650,-0.0155781375617193,-0.00809674359268633];
% BaseUpsamplingRateTx = 4;
% FilterRRCTxTmp = zeros(1,6*BaseUpsamplingRateTx-1);
% tmpF = rcosdesign(0.485,6,BaseUpsamplingRateTx,'sqrt');
% FilterRRCTxTmp = tmpF(1,2:end-1)./(max(tmpF));

FilterRRCTxTmp1 = (FilterRRCTxTmp(1,floor(length(FilterRRCTxTmp)/2))*2 + FilterRRCTxTmp(1,ceil(length(FilterRRCTxTmp)/2)))/3;
FilterRRCTxTmp2 = [FilterRRCTxTmp(1,1:floor(length(FilterRRCTxTmp)/2)) FilterRRCTxTmp1];
FilterRRCTxTmp3 = [FilterRRCTxTmp2 fliplr(FilterRRCTxTmp2)];

h = [zeros(1,(8192-length(FilterRRCTxTmp3))/2) FilterRRCTxTmp3  zeros(1,(8192-length(FilterRRCTxTmp3))/2)];

HC = conj(fft(h)/sqrt(winLen));

if bool == 0
    HC = ones(1,filterLen);
else
    HC = [HC(end-halfBL+1:end) HC(1:halfBL)] * sqrt(80);
end

%New HJH ADD 20220627
htmp = [-0.0009765625,-0.0029296875,-0.0029296875,0.0009765625,0.0048828125,0.00390625,-0.001953125,-0.0068359375,-0.005859375,0.0009765625,0.0078125,0.0087890625,0.001953125,-0.0078125,-0.009765625,-0.0009765625,0.0107421875,0.0126953125,-0.001953125,-0.0234375,-0.0302734375,-0.005859375,0.041015625,0.07421875,0.0517578125,-0.03515625,-0.1416015625,-0.1796875,-0.0693359375,0.2060546875,0.568359375,0.8779296875,1,0.8779296875,0.568359375,0.2060546875,-0.0693359375,-0.1796875,-0.1416015625,-0.03515625,0.0517578125,0.07421875,0.041015625,-0.005859375,-0.0302734375,-0.0234375,-0.001953125,0.0126953125,0.0107421875,-0.0009765625,-0.009765625,-0.0078125,0.001953125,0.0087890625,0.0078125,0.0009765625,-0.005859375,-0.0068359375,-0.001953125,0.00390625,0.0048828125,0.0009765625,-0.0029296875,-0.0029296875,-0.0009765625];
htmp = [ 0 htmp];
hTmp1 = upsample(htmp,2); % 2: 128kHz/16kHz*4
HCE = 9;
h = [zeros(1,winLen/2-length(hTmp1)/2) hTmp1 zeros(1,winLen/2-length(hTmp1)/2)];
HC = conj(fft(h)/sqrt(winLen)); % figure; plot(semilogy(abs(HC).^2));
HC = [HC(end-halfBL+1:end) HC(1:halfBL)]; % figure; plot(semilogy(abs(HC).^2));
HC = HC*HCE;

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
        signal1 = signalFft(i,:);
%         signal1 = signalFft(i,:) .* HC;
        % sum(abs(signalFft(i,:)).^2)/sum(abs(signal1).^2)
%         signal2 = [signal1(halfBL+1:end) zeros(1,ifftLen-halfBL*2) signal1(1:halfBL)]*exp((1i)*2*pi*freqOffsetChGrid(i)*(j-1)*overlapping/winLen);
        signal2 = [signal1(halfBL+1:end) zeros(1,ifftLen-halfBL*2) signal1(1:halfBL)];
%         signal2 = [signal1(halfBL+1:end) zeros(1,ifftLen-halfBL*2) signal1(1:halfBL)]*exp((1i)*2*pi*freqOffsetChGrid(ceil(i/(supressNum+1)))*(j-1)*(overlapping)/winLen); %IFFT的输入需要低频在前高频在后，矫正由于重叠点造成的相位偏差？
%         signal3 = ifft(signal2,ifftLen)*sqrt(ifftLen);
        signal3 = ifft(signal2,ifftLen)*ifftLen;
        signalArr(i,(j-1)*ifftLen+1:j*ifftLen) = signal3;
    end
end


% for j = 1:slideTime
%     % 取出一段数据
%     signalFft = signalArr2(:,(j-1)*filterLen+1:j*filterLen);%取出每次滤波长度频域数据
%     for i = 1:numCh*(supressNum+1)
%         signal1 = signalFft(i,:) .*HC *sqrt(6.6);    % 原来匹配滤波在这  匹配滤波进行能量归一化6.6是经验值
% %         signal1 = signalFft(i,:) .*HC;    % 原来匹配滤波在这  匹配滤波进行能量归一化6.6是经验值
%         signal2 = [signal1(halfBL+1:end) zeros(1,ifftLen-halfBL*2) signal1(1:halfBL)]*exp((1i)*2*pi*freqOffsetChGrid(ceil(i/(supressNum+1)))*(j-1)*(overlapping)/winLen); %IFFT的输入需要低频在前高频在后，矫正由于重叠点造成的相位偏差？
%         signal3 = ifft(signal2,ifftLen)*sqrt(ifftLen); %频域到时域
%         if j == 1
%             signal4 = [signal3(rightStart-margin:end) signal3(1:leftEnd)]; % ifft结果的时间关系，需要重新排列，保证序列按照时间顺序排列，第一次滑动保留前16点
%             signalArr3(i,1:BlkLen1) = signal4;
%         elseif j == slideTime
%             signal4 = [signal3(rightStart:end) signal3(1:leftEnd+margin)];%第三次滑动保留后16点
%             signalArr3(i,(j-2)*BlkLen+BlkLen1+1:(j-2)*BlkLen+BlkLen1+BlkLen1) = signal4;
%         else
%             signal4 = [signal3(rightStart:end) signal3(1:leftEnd)]; %第二次滑动前16点和后16点都丢弃
%             signalArr3(i,(j-2)*BlkLen+BlkLen1+1:(j-1)*BlkLen+BlkLen1) = signal4;
%         end
%     end
% end

signalArr = signalArr(:,1:realLen/(fs/fs2));

end

