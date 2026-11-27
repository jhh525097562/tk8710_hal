
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

% 统计干净的滤波后MSK信号+不同功率的复高斯白噪声的模的序列的平均功率P,对序列用P归一化后求均值E,建立E与MSK信号SNR的对应表格

segment = 3;                % 滑动次数
frameLen = winLen*segment - (segment-1)*overlapping;    % 一帧的采样点数
LS = ceil(frameLen/fs);     % 仿真码元个数

frameNumber = 200;          % 帧数

load('SNRTable');

M = 4;
N = 8;
SNR = -20:1:4;
SNR = 99;
supressNum = 7;             % 抑制分量个数

PCoef = -99;                  % 功率差
HCoef = sqrt(0.1);          % 信道估计误差
alpha = [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9];               % 恒模权重
PeCoef = 1;                 % 功率系数

errSelect = zeros(length(SNR),length(alpha));
errAll = zeros(length(SNR),1);
errMMSE = zeros(length(SNR),1);

h = geth(fs);
% 处理每一帧数据
for ii = 1:frameNumber

    symbols = randi([0 1],LS,M);
    symbols(symbols==0) = -1;
    modSignal = zeros(LS*fs,M);
    info = symbols;
    info(2:2:end,:) = info(2:2:end,:)*(1i);
    infoup = upsample(info,fs);
    for mm = 1:M
        temp = conv(infoup(:,mm),h);
        modSignal(:,mm) = temp(2*fs+1:end-2*fs+1);
    end
    MSK = transpose(modSignal);

    % 生成随机频偏
    freq = randi([-sampleFrequency/2 sampleFrequency/2],1);
    freqInt = round(freq/sampleFrequency*winLen)/winLen*sampleFrequency;
    
    % 生成发送信号
    Tx = MSK.*repmat(exp((1i)*2*pi*(0:LS*fs-1)/sampleFrequency*freqInt),M,1);
    Tx = Tx(:,1:frameLen);
    
    % 生成信道响应
    H = (randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N);
    % 信道估计误差
    HDiff = (randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N);
    
    % 仿真不同信噪比
    for jj = 1:length(SNR)
        % 噪声功率
        sigma = sqrt(1/(10^(SNR(jj)/10)));
        % 噪声序列
        noise = sigma*(randn(N,frameLen)+randn(N,frameLen)*(1i))/sqrt(2);
        
        % 仿真不同功率差
        for kk = 1:length(PCoef)
            if PCoef(kk) == 1
                D = eye(M);
            else
                D = diag(10.^((PCoef(kk)+20*rand(1,M))/20));
                D(1,1) = 1;
            end
            
            % 接收信号
            Rx = H*D*Tx+noise;
            
            % 仿真不同的信道误差
            for ll = 1:length(HCoef)
                Hhat = (H + HCoef(ll)*HDiff)*D;
                
                % 信道化
                dataRec1 = channelize20180408A(Rx, repmat(freqInt,1,M), overlapping, winLen, filterLen, sampleFrequency);
                dataRec2 = channelize20180408B(dataRec1, Hhat, filterLen, N, supressNum);
                dataRec3 = channelize20180408C(dataRec2, repmat(freqInt,1,M*(supressNum+1)), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1);
                
                % 选择算法，里仅处理第一个用户
                if M == 1
                    dataRec = dataRec3(1:N,:)/1.8162;
                else
                    % 恒模和功率选择算法
                    dataRec4 = dataRec3(1:N,:)/1.8162;
                    
                    % 求E
                    dataAbs = abs(dataRec4);
                    P = dataAbs*dataAbs'/length(dataAbs);
                    PP = diag(sqrt(P));
                    dataNormal = diag(1./PP)*dataAbs;
                    E = mean(dataNormal,2);
                    
                    powExp = sum(abs(Hhat(:,1)).^2);
                    % 查SNR
                    select = zeros(N,length(alpha));
                    for mm = 1:N
                        diff = abs(E(mm)-SNRTable(:,3));
                        [temp,index] = min(diff);
                        SNRSelect1 = SNRTable(index,2);
                        Pmsk = P(mm,mm)*10^(SNRSelect1/10)/(10^(SNRSelect1/10)+1);
                        select(mm,:) = alpha*(SNRSelect1) - (1-alpha)*abs(10*log10(Pmsk/powExp));
                    end

                    [~,index] = max(select);
                    dataRec = dataRec4(index,:);
                end
                
                % 计算误码率
                MSKReal = real(MSK(1,2*fs+1:2*fs:end-fs));
                MSKImag = imag(MSK(1,fs+1:2*fs:end-fs));
                dataReal1 = real(dataRec(:,2*fs2+1:2*fs2:end-fs2));
                dataReal1(dataReal1>=0) = 1;
                dataReal1(dataReal1<0) = -1;
                dataImag1 = imag(dataRec(:,fs2+1:2*fs2:end-fs2));
                dataImag1(dataImag1>=0) = 1;
                dataImag1(dataImag1<0) = -1;
                err1 = (sum(abs(repmat(MSKReal,length(alpha),1)-dataReal1),2) + sum(abs(repmat(MSKImag,length(alpha),1)-dataImag1),2))/2;
                errSelect(jj,:) = errSelect(jj,:) + err1' / (LS-2) / frameNumber;
                
                dataRec1 = channelize20180408A(Rx, repmat(freqInt,1,M), overlapping, winLen, filterLen, sampleFrequency);
                dataRec2 = compareBest(dataRec1(1,:), filterLen, N, Hhat);
                dataRec3 = channelize20180408C(dataRec2, repmat(freqInt,1,2^supressNum), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1)/2;
                dataReal1 = real(dataRec3(:,2*fs2+1:2*fs2:end-fs2));
                dataReal1(dataReal1>=0) = 1;
                dataReal1(dataReal1<0) = -1;
                dataImag1 = imag(dataRec3(:,fs2+1:2*fs2:end-fs2));
                dataImag1(dataImag1>=0) = 1;
                dataImag1(dataImag1<0) = -1;
                errAll(jj,1) = errAll(jj,1) + min(sum(abs(repmat(MSKReal,128,1)-dataReal1),2) + sum(abs(repmat(MSKImag,128,1)-dataImag1),2))/2 / (LS-2) / frameNumber;

                RxMMSE = (Hhat'*Hhat+sigma^2*10^(-1.505)*eye(M))\Hhat'*Rx;
                dataRec1 = channelize20180408A(RxMMSE(1,:), freqInt, overlapping, winLen, filterLen, sampleFrequency);
                dataRec3 = channelize20180408C(dataRec1, repmat(freqInt,1,supressNum+1), filterLen, fs, fs2, overlapping, winLen, sampleFrequency, size(Rx,2),1)/2;
                dataReal1 = real(dataRec3(1,2*fs2+1:2*fs2:end-fs2));
                dataReal1(dataReal1>=0) = 1;
                dataReal1(dataReal1<0) = -1;
                dataImag1 = imag(dataRec3(1,fs2+1:2*fs2:end-fs2));
                dataImag1(dataImag1>=0) = 1;
                dataImag1(dataImag1<0) = -1;
                errMMSE(jj,1) = errMMSE(jj,1) +(sum(abs(MSKReal-dataReal1)) + sum(abs(MSKImag-dataImag1)))/2 / (LS-2) / frameNumber;
                
            end
        end
    end
    if mod(ii,10) == 0
        fprintf('alpha: %f, frame number: %d\n',alpha(1),ii);
    end
end

figure
semilogy(errSelect);

figure
semilogy(SNR,errSelect,'-ob',SNR,errAll,'-*r',SNR,errMMSE,'-*b');  legend('our','2^7','MMSE');    grid on;
xlabel('SNR');  ylabel('error rate');   title('error rate vs SNR under imperfect CSI');

toc



















