
clear all
close all

tic

% user number
M = 4;
% receiver antenna number
N = 8;
% frame number
FrameNumber = 1000;
% frame length
FrameLength = 256;
% signal to noise ratio
SNR = -6:1:14;
% 信道误差
HCoef = sqrt(0.1);
% 功率差
PCoef = [10 0 -10 -20 -30 1];
% 功率系数
PeCoef = [1,2,4];
% SNR权重
alpha = 0.8;

load('SNRTable.mat');
BPSKTable = SNRTable;

for hh = 1:length(PCoef)
    
    errorSigma = zeros(length(SNR),length(PeCoef));
    errorAll = zeros(length(SNR),1);
    
    % large scale fading
    if PCoef(hh) == 1
        D = eye(M);
    else
        D = diag(10.^((PCoef(hh)+20*rand(1,M))/20));
        D(1,1) = 1;
    end

    for ii = 1:length(SNR)
        % noise variance
        sigma = sqrt(1/(10^(SNR(ii)/10)));

        for jj = 1:FrameNumber
            % bit symbols
            symbols = randi([0,1],M,FrameLength)*2-1;
            % channel response
            H = (randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N);
            % noise
            noise = sigma*(randn(N,FrameLength)+randn(N,FrameLength)*(1i))/sqrt(2);
            % receive signal
            Rx = H*D*symbols+noise;
            % channel estimation
            Hhat = (H + HCoef*(randn(N,M)+randn(N,M)*(1i))/sqrt(2)/sqrt(N))*D;

            % space depressing decoder
            for kk = 1:1
                HNormal = Hhat(:,kk)/sqrt(sum(abs(Hhat(:,kk).^2)));
                inphaseComponent = HNormal'*Rx;
                orthogonalComponent = Rx - HNormal*inphaseComponent;
                R = orthogonalComponent*orthogonalComponent';
                [U,G,V] = svd(R);
                
                % sigma
                inp = inphaseComponent;
                SDDecoder = zeros(N,FrameLength);
                SDDecoder(1,:) = inp;
                for ll = 1:N-1
                    S = V(:,ll)'*orthogonalComponent; 
                    A = inp*S'/(S*S');
                    inp = inp - A*S;
                    SDDecoder(1+ll,:) = inp;
                end
                % 选择算法
                % 求E
                dataAbs = abs(SDDecoder);
                P = dataAbs*dataAbs'/length(dataAbs);
                PP = diag(sqrt(P));
                dataNormal = diag(1./PP)*dataAbs;
                E = mean(dataNormal,2);
                % 求功率
                powExp = sum(abs(Hhat(:,kk)).^2);
                % 查SNR
                select = zeros(N,length(PeCoef));
                for mm = 1:N
                    diff = abs(E(mm)-BPSKTable(:,3));
                    [temp,index] = min(diff);
                    SNRSelect1 = BPSKTable(index,2);
                    Pmsk = P(mm,mm)*10^(SNRSelect1/10)/(10^(SNRSelect1/10)+1);
                    select(mm,:) = alpha*(SNRSelect1) - (1-alpha)*abs(10*log10(PeCoef*Pmsk/powExp)).^2;
                end
                [~,index] = max(select);
                SDcode = real(SDDecoder(index,:));
                SDcode(SDcode>=0) = 1;
                SDcode(SDcode<0) = -1;
                % calculate error
                errorTemp = sum(abs(repmat(symbols(kk,:),length(index),1)-SDcode)/2,2);
                errorSigma(ii,:) = errorSigma(ii,:) + errorTemp'/FrameNumber/FrameLength;
                
                % all
                SDDecoder = zeros(2^(N-1),FrameLength);
                for mm = 1:2^(N-1)
                    indexLag = dec2bin(mm-1) == '1';
                    index = find(indexLag);
                    len = length(index);
                    inp = inphaseComponent;
                    for ll = 1:len
                        S = V(:,index(ll))'*orthogonalComponent; 
                        A = inp*S'/(S*S');
                        inp = inp - A*S;
                    end
                    SDDecoder(mm,:) = inp;
                end
                SDcode = real(SDDecoder);
                SDcode(SDcode>=0) = 1;
                SDcode(SDcode<0) = -1;
                % calculate error
                errorTemp = min(sum(abs(repmat(symbols(kk,:),2^(N-1),1)-SDcode)/2,2));
                errorAll(ii,:) = errorAll(ii,:) + errorTemp/FrameNumber/FrameLength;
            end
        end
        PCoef(hh)
        SNR(ii)
    end
    subplot(2,3,hh); semilogy(SNR,errorSigma(:,1),'-*b',SNR,errorSigma(:,2),'-ob',SNR,errorSigma(:,3),'-.b',SNR,errorAll,'-*r');
    legend(num2str(PeCoef(1)),num2str(PeCoef(2)),num2str(PeCoef(3)),'2^7');
    title(['功率干扰强度' num2str(PCoef(hh)) 'db']);
    grid on
end

toc


