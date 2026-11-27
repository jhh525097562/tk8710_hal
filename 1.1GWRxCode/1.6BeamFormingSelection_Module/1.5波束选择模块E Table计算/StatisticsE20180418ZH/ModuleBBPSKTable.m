
clear all
close all

tic


LS = 256;

frameNumber = 200;          % 帧数
SNR = -10:1:39;             % 信噪比


SNRCheck = zeros(frameNumber,length(SNR));
ECount = zeros(frameNumber,length(SNR));
BPSKTable = zeros(length(SNR),2);
BPSKTable(:,1) = SNR';

for ii = 1:frameNumber
    
    % 生成符号
    bits = randi([0 1],LS,1);
    symbols = 2*bits - 1;
    
    symbolsPower = symbols'*symbols;
    
    for jj = 1:length(SNR)
        
        % 生成噪声
        sigma = sqrt(1/(10^(SNR(jj)/10)));
        noise = sigma*(randn(1,length(symbols))+(1i)*randn(1,length(symbols)))/sqrt(2);
        
        noisePower = noise*noise';
        
        SNRHat = 10*log10(symbolsPower/noisePower);
        SNRCheck(ii,jj) = SNRHat;
        
        % 统计
        data = symbols' + noise;
        dataAbs = abs(data);
        P = dataAbs*dataAbs'/length(dataAbs);
        dataNormal = dataAbs/sqrt(P);
        dataAbs0 = abs(data).^2;
        p0 = sum(dataAbs0)/length(dataAbs0);
        dataNormal0 = (dataAbs0./p0).^2;
        E = mean(dataNormal0);
        ECount(ii,jj) = E;
 
    end

end

BPSKTable(:,2) = mean(SNRCheck);
BPSKTable(:,3) = mean(ECount);

save('BPSKTable20221019.mat','BPSKTable')

toc



















