
function [SigRxOut] = GWRxBFModule(SlideIndex,UserIndex,FFTIn,AH,freqOffsetChGrid,Antenna,FFTLen,Mfsk,filterLen,IFFTLen,AddZerosNum,GWRXBFMode,CaseNum)
% ?????????????????
halfBL = filterLen/2;                           % ????????????

if Mfsk == 4
%     tmpFilter125 = designfilt('lowpassfir', 'FilterOrder', 6, 'CutoffFrequency', 3595, 'SampleRate', 16000,'StopbandAttenuation',25); % best for dpfsk_signalTransition_plusFiltering with 16 samples per symbol
%     htmp = tmpFilter125.Coefficients;
%     htmp = [ 0 htmp];
    htmp = [0,-0.0634756387831438,0.0368512994716392,0.303904820085406,0.463389896180281,0.303904820085406,0.0368512994716392,-0.0634756387831438];
    hTmp1 = upsample(htmp,AddZerosNum); % 8: 128kHz/8kHz
    HCE = 1;
end

h = [zeros(1,FFTLen/2-length(hTmp1)/2) hTmp1 zeros(1,FFTLen/2-length(hTmp1)/2)];
% HC = conj(fft(h)/sqrt(FFTLen)); % figure; plot(semilogy(abs(HC).^2));
HC = conj(fft(h)); % figure; plot(semilogy(abs(HC).^2));
HC = [HC(end-halfBL+1:end) HC(1:halfBL)]; % figure; plot(semilogy(abs(HC).^2));
HC = HC*HCE;

%% ??????
% freqOffsetChGrid = round((freqOffsetCh+GWRxSampleRate)/GWRxSampleRate*FFTLen);
signal1 = circshift(FFTIn.',-freqOffsetChGrid);
sigSeg = ([signal1(end-halfBL+1:end,:); signal1(1:halfBL,:)].').*repmat(HC,Antenna,1);
if GWRXBFMode==0
    HNormal = AH(:,1)/sqrt(sum(abs(AH(:,1).^2)));
    inphaseComponent = HNormal'*sigSeg;
    orthogonalComponent = sigSeg - HNormal*inphaseComponent;
    R = orthogonalComponent*orthogonalComponent';
    [U,G,V] = svd(R);
    [U2 G2 V2] = svd_C(R);
    G = G2;
    V = U2;
%     [SNR,G_Dif,C_G,C_V,C_BFOut] = SVD_Compare(R,G,V,UserIndex,SlideIndex,CaseNum);
%     G = C_G;
%     V = C_V;

%     SNR
%     G_Dif
%     [U,G,V] = SVD_Test(R);
    SDDecoder = zeros(Antenna-1,filterLen);
    SDDecoder(1,:) = inphaseComponent;
    S = V'*orthogonalComponent;
    %         tempS = diag(abs(S(1:Antenna,:)*S(1:Antenna,:)')).';
    tempS = diag(G).';
    for kk = 1:(Antenna-1)
        % add 20180724
        if abs(tempS(kk)) < eps
            A = 0;
        else
            A = inphaseComponent*S(kk,:)'/tempS(kk);
        end
        TmpA(kk) = abs(A).^2;
        inphaseComponent = inphaseComponent - A*S(kk,:);
        SDDecoder(1+kk,:) = inphaseComponent;
    end
    dataOut(1:Antenna,1:IFFTLen) = [SDDecoder(:,halfBL+1:end) zeros(Antenna,IFFTLen-halfBL*2) SDDecoder(:,1:halfBL)];
elseif GWRXBFMode==1 || GWRXBFMode==2
    dataOut(1:Antenna,1:IFFTLen) = [sigSeg(:,halfBL+1:end) zeros(Antenna,IFFTLen-halfBL*2) sigSeg(:,1:halfBL)];
end
%% IFFT Module
SigRxOut = zeros(Antenna,IFFTLen);
SigRxOut = ifft(dataOut(1:Antenna,1:IFFTLen).')*IFFTLen;
SigRxOut = SigRxOut.';
SigRxOut = [SigRxOut(:,IFFTLen/2+1:end) SigRxOut(:,1:IFFTLen/2)];
if 1
%     for k=1:Antenna
%        SigRxOut(k,:) =  SigRxOut(k,:)/sqrt(sum(abs(SigRxOut(k,1:end).^2)));
%     end
    SigRxOut = SigRxOut./IFFTLen;
end
% figure;plot(real(C_BFOut(1,:)));hold on;plot(real(SigRxOut(1,:)));legend('C','M')
end
