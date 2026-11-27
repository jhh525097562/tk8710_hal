function [SigRxOut, ANoise_o, APreambleFftETmp_o] = FFTModule(SigRxIn,Antenna,FFTLen, flag_cal_a_noise, GWRXPara,Iter)

persistent ANoise APreambleFftETmp
global ANoise0;
SigRxOut = zeros(Antenna, FFTLen);

SigRxOut = fft(SigRxIn(1:Antenna,1 : FFTLen).');
SigRxOut = SigRxOut.';
[M, ~] = size(SigRxOut);
SigRxOut(1 : M, 1) = SigRxOut(1 : M, 2);
%% 每根天线的噪底计算（软件实现）
if (flag_cal_a_noise)
    SigRxOut_cal = SigRxOut;
    SigRxOut_cal(1 : M, 1) = SigRxOut_cal(1 : M, 2);
    APreambleFftETmp = (abs(SigRxOut_cal)).^2;
    [ANoise] = AnTennaGainAdjustModule(APreambleFftETmp, M, FFTLen, GWRXPara);
%     [ANoise] = AnTennaGainAdjustModule_Test(APreambleFftETmp,Antenna,FFTLen,GWRXPara);
    if Iter==1
    else
        for j=1:M
            if ANoise(j,1) > ANoise0(j,1)
                ANoise(j,1) = ANoise0(j,1) * (1+GWRXPara.GWRX_ANoiseUpdataPer);
            else
                ANoise(j,1) = ANoise0(j,1) * (1-GWRXPara.GWRX_ANoiseUpdataPer);
            end
        end
    end
    ANoise0 = ANoise;
end
ANoise_o = ANoise;
ANoise_t = ANoise./sum(ANoise);
% Tmp = sort(ANoise);
% ANoise_o = ANoise/Tmp(ceil(M/2));
APreambleFftETmp_o = APreambleFftETmp;

% APreambleFftESum1Tmp = APreambleFftETmp_o ./ ANoise(:,1) ./ FFTLen^2;
APreambleFftESum1Tmp = APreambleFftETmp_o ./ ANoise_t(:,1);
APreambleFftESum1Tmp1 = sum(APreambleFftESum1Tmp(1:M,:),1);
APreambleFftETmp_o = [APreambleFftESum1Tmp1(:, FFTLen/2+1:FFTLen) APreambleFftESum1Tmp1(:,1:FFTLen/2)];

%% 增益刻度调整
% SigRxOut=  SigRxOut ./ sqrt(ANoise(:,1)) ./ FFTLen;
SigRxOut=  SigRxOut ./ sqrt(ANoise_t(:,1));
end
