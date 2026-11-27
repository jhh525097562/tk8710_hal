
CaseNum = 177;
AntennaNum = 8;
%% 
folderPath = ['/home/ALG01/work/K2301/alg/c_model_Tmp/testbench_ud/c_data/case',num2str(CaseNum)];
if ~exist(folderPath,'dir')
    mkdir(folderPath);
end
%% UD Antenna Data
filename = ['/home/ALG01/work/K2301/alg/c_model_Tmp/testbench_ud/c_data/case',num2str(CaseNum),'/UD_InData.txt']; 
RxSignalOneSlot1 = reshape(SigDownSampleRx(:,1:FftSize*slideTime),[FftSize*8*slideTime 1]);
write2data(RxSignalOneSlot1,filename,'txt');

filename1 = ['/home/ALG01/work/K2301/alg_HJH/c_model_Tmp/testbench_ud/c_data/case',num2str(CaseNum),'/UD_SetFreq.txt']; 
TmpSetFreq = zeros(1,128);
TmpSetFreq(1:length(GWRXPara.AllocationRXFreqSet)) = GWRXPara.AllocationRXFreqSet;
TmpSetFreq1 = reshape(TmpSetFreq,[GWRxAllUser 1]);
write2data(TmpSetFreq1,filename1,'txt');
%% C_model Data
filename = ['/home/data/K2301/K2301PHY_CaseList_Data/UDModule_Data/case',num2str(CaseNum),'/TestCmodel.dat'];
file_o_in_T1 = load(filename);

%% RemoveDC
clear fileIn;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).';
    figure;plot(abs(SigRemoveADCOut(it,:) - fileIn(it,:))./abs(SigRemoveADCOut(it,:)))
end

%% DC
clear fileIn;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1((it-1)*2+1,1) + file_o_in_T1(it*2,1)*1i);
end
figure;plot(real(ADC_REG(:,1) - fileIn(:,1)));
%% fft
clear fileIn;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).';
    figure;plot(abs(SigRxOut(it,:) - fileIn(it,:))./abs(SigRxOut(it,:)));
    figure;plot(real(SigRxOut(it,:)));hold on;plot(real(fileIn(it,:)))
end

% APreambleFftETmp
clear fileIn;
close all;
for it=1:AntennaNum
    fileIn(it,:) = file_o_in_T1((it-1)*1+1:8:end,1);
    figure;plot(abs(APreambleFftETmp(it,:) - fileIn(it,:))./abs(APreambleFftETmp(it,:)));
    figure;plot(real(APreambleFftETmp(it,:)));hold on;plot(real(fileIn(it,:)))
end

%% Anoise
clear fileIn;
fileIn = file_o_in_T1;
figure;plot(abs(ANoise - fileIn)./abs(ANoise));

%% FFTEsum
fileIn = file_o_in_T1.';
figure;plot(abs(APreambleFftESum1 - fileIn)./abs(APreambleFftESum1));

%% FFT/Anoise
clear fileIn;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).';
    figure;plot(abs(FFTOut(it,:) - fileIn(it,:))./abs(FFTOut(it,:)))
    figure;plot(real(FFTOut(it,:)));hold on;plot(real(fileIn(it,:)));legend('M','C')
end

%% P1 THE Result
fileIn = file_o_in_T1.';
figure;plot(abs(The - fileIn)./abs(The));
figure;plot(real(The(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')
figure;plot(10*log10(APreambleFftESum1));hold on;plot(10*log10(The*K));hold on;plot(10*log10(fileIn*K));legend('M0','M','C')
% p1 APreambleFftESum1
fileIn = file_o_in_T1.';
figure;plot(abs(APreambleFftESum1 - fileIn)./abs(APreambleFftESum1));
figure;plot(real(APreambleFftESum1(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')
%% P1
fileIn = file_o_in_T1.';
figure;plot(abs(P1_o - fileIn))
figure;plot(real(P1_o(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% Highlightpos pos_o
fileIn = file_o_in_T1.';
figure;plot(abs(pos_o - fileIn))
figure;plot(real(pos_o(1,:)-1));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% P3 
fileIn = file_o_in_T1.';
% fileIn = file_o_in_T1(2049:end).';
figure;plot(abs(P3 - fileIn))
figure;plot(real(P3(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% P4
% fileIn = file_o_in_T1.';
fileIn = file_o_in_T1(1:end).';
% Tmp = fileIn(275:end-1);
% fileIn(276:end) = Tmp;
figure;plot(abs(P4 - fileIn))
figure;plot(real(P4(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% P5
fileIn = file_o_in_T1.';
% Tmp = fileIn(277:end-1);
% fileIn(278:end) = Tmp;
figure;plot(abs(P5 - fileIn))
figure;plot(real(P5(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% P6
fileIn = file_o_in_T1.';
% Tmp = fileIn(14:end);
% fileIn(13:end-1) = Tmp;
figure;plot(abs(P6 - fileIn))
figure;plot(real(P6(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% P7
fileIn = file_o_in_T1.';
% Tmp = fileIn(476:end);
% fileIn(475:end-1) = Tmp;
% fileIn(473:474) = P7(473:474);
figure;plot(abs(P7 - fileIn))
figure;plot(real(P7(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

%% AfreqBinMax  AfreqBin
fileIn = file_o_in_T1.';
C_AfreqBinMax = fileIn(1:GWRxAllUser);
C_AfreqBin = fileIn(GWRxAllUser+1:end);
figure;plot(sort(FreqPosMax(1,:))-1);hold on;plot(sort(C_AfreqBinMax(1,:)));legend('M','C')
figure;plot(real(FreqPos(1:GWRxAllUser)));hold on;plot(real(C_AfreqBin(1,:)));legend('M','C')

%% Freq AH
fileIn = file_o_in_T1.';
C_Freq = fileIn(1:128);
C_AH = (fileIn(128+1:2:end) + fileIn(128+2:2:end)*1i);
M_AH = reshape(AH_REG,[8*128 1]).';
figure;plot(Freq_REG - C_Freq);
figure;plot(abs(M_AH -C_AH));

%% %%%%%%%%%%IS Module %%%%%%%%%%%%%%%%%
CaseNum = 97;
AntennaNum = 8;
%%
filename = ['/home/ALG01/work/K2301/alg/c_model_Tmp/testbench_is/c_data/case',num2str(CaseNum),'/TestCmodel.dat'];
file_o_in_T1 = load(filename);

% HC
fileIn = file_o_in_T1.';
C_HC = (fileIn(1:2:end) + fileIn(2:2:end)*1i);
figure;plot(abs(HC - C_HC))
figure;plot(real(HC(1,:)));hold on;plot(real(C_HC(1,:)));legend('M','C')

% sigSeg
clear fileIn;
close all;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).';
    figure;plot(abs(sigSeg(it,:) - fileIn(it,:)))
    figure;plot(real(sigSeg(it,:)));hold on;plot(real(fileIn(it,:)));legend('M','C')
end

% HNormal AH
UserNum = 0;
StartPos = UserNum*2888;
clear fileIn;
fileIn = (file_o_in_T1(StartPos+1:2:StartPos+16,1) + file_o_in_T1(StartPos+2:2:StartPos+16)*1i);
figure;plot(abs(AH - fileIn))
figure;plot(imag(AH));hold on;plot(imag(fileIn));legend('M','C')

%inphaseComponent
clear fileIn;
fileIn = (file_o_in_T1(16+1:2:320*2+16,1) + file_o_in_T1(16+2:2:320*2+16)*1i).';
figure;plot(abs(inphaseComponent - fileIn)./abs(inphaseComponent))
figure;plot(real(inphaseComponent));hold on;plot(real(fileIn));legend('M','C')
%orthogonalComponent
clear fileIn;
close all;
for it=1:AntennaNum
    fileIn(it,:) = (file_o_in_T1(656+(it-1)*2+1:16:end,1) + file_o_in_T1(656+(it-1)*2+2:16:end)*1i).';
    figure;plot(abs(orthogonalComponent(it,:) - fileIn(it,:))./abs(orthogonalComponent(it,:)))
    figure;plot(real(orthogonalComponent(it,:)));hold on;plot(real(fileIn(it,:)));legend('M','C')
end
% R G V
clear fileIn;
close all;
UserNum = 2;
startPos = UserNum*320;
file_o_in_T2 = file_o_in_T1(startPos+1:64*2+startPos);
file_o_in_T3 = file_o_in_T1(startPos+128+1:startPos+64+128);
file_o_in_T4 = file_o_in_T1(startPos+192+1:startPos+64*2+192);
for it=1:AntennaNum
    C_R(it,:) = ((file_o_in_T2((it-1)*2+1:16:end,1) + file_o_in_T2((it-1)*2+2:16:end)*1i).');
    figure;plot(abs(R(it,:) - C_R(it,:))./abs(R(it,:)))
    figure;plot(real(R(it,:)));hold on;plot(real(C_R(it,:)));legend('M','C')
end
for it=1:AntennaNum
    C_G(it,:) = ((file_o_in_T3((it-1)*8+1:8*it,1) + 0*1i).');
end
figure;plot(abs(diag(G) - diag(real(C_G))))
figure;plot(real(diag(G)));hold on;plot(real(diag(C_G)));legend('M','C')

for it=1:AntennaNum
    C_V(it,:) = ((file_o_in_T4((it-1)*2+1:16:end,1) + file_o_in_T4((it-1)*2+2:16:end)*1i).');
%     figure;plot(abs(V(i,:) - C_V(i,i)).^2./abs(V(i,:)).^2)
%     figure;plot(real(V(i,:)));hold on;plot(real(C_V(i,:)));legend('M','C')
end
snr_temp = [snr_lineva(V(:, 1), C_V(:, 1),0) ...
snr_lineva(V(:, 2), C_V(:, 2),0) ...
snr_lineva(V(:, 3), C_V(:, 3),0) ...
snr_lineva(V(:, 4), C_V(:, 4),0) ...
snr_lineva(V(:, 5), C_V(:, 5),0) ...
snr_lineva(V(:, 6), C_V(:, 6),0) ...
snr_lineva(V(:, 7), C_V(:, 7),0) ...
snr_lineva(V(:, 8), C_V(:, 8),0)]

% TmpAS
clear C_Data;
for it=1:AntennaNum-1
    C_Data(it,:) = ((file_o_in_T1((it-1)*2+1:14:end,1) + file_o_in_T1((it-1)*2+2:14:end)*1i).');
%     figure;plot(abs(TmpAS(i,:) - C_Data(i,:))./abs(TmpAS(i,:)))
    figure;plot(real(TmpAS(it,:)));hold on;plot(real(C_Data(it,:)));legend('M','C')
end

CaseNum = 225;
AntennaNum = 8;
filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/BFModuleOut.dat'];
file_SVDIn_R = load(filename);
% SigRxOut 
for it=1:AntennaNum
    C_Data(it,:) = ((file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).');
    figure;plot(abs(dataOut(it,:) - C_Data(it,:))./abs(dataOut(it,:)))
    figure;plot(real(dataOut(it,:)));hold on;plot(real(C_Data(it,:)));legend('M','C')
end

for it=1:AntennaNum
    C_Data(it,:) = ((file_o_in_T1((it-1)*2+1:16:end,1) + file_o_in_T1((it-1)*2+2:16:end)*1i).');
    figure;plot(abs(SigRxOut(it,:) - C_Data(it,:))./abs(SigRxOut(it,:)))
    figure;plot(real(SigRxOut(it,:)));hold on;plot(real(C_Data(it,:)));legend('M','C')
end
%% UD Result

CaseNum = 225;
AntennaNum = 8;
FreamNum = 1;

filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/BFModuleOut.dat'];
file_BFModule_Out = load(filename);
file_tmp = file_BFModule_Out(49152+1:end);
clear fileIn;
for it=1:AntennaNum
    fileIn(it,:) = (file_tmp((it-1)*2+1:16:end,1) + file_tmp((it-1)*2+2:16:end)*1i).';
    figure;plot(abs(SigRxIn(it,:) - fileIn(it,:))./abs(SigRxIn(it,:)))
    figure;plot(real(SigRxIn(it,:)));hold on;plot(real(fileIn(it,:)));legend('M','C')
end


filename = ['/home/data/K2301/K2301PHY_CaseList_Data/UDModule_Data/case',num2str(CaseNum),'/UD_FFT_Out.dat'];
file_UD_FFTOut = load(filename);

filename = ['/home/ALG01/work/K2301/alg/c_model_Tmp/testbench_ud/c_data/case',num2str(CaseNum),'/UD_P7.dat'];
file_UD_P7 = load(filename);

filename = ['/home/ALG01/work/K2301/alg/c_model_Tmp/testbench_ud/c_data/case',num2str(CaseNum),'/UD_Out.dat'];
file_UD_Out = load(filename);

% fft out
file_tmp = file_UD_FFTOut((FreamNum-1)*393216+1:393216*FreamNum);
% file_tmp = file_UD_FFTOut((FreamNum-1)*393216+1:131072*FreamNum);
clear fileIn;
for it=1:8
%     a(i,:)=fft(SigRemoveADCOut(i,1 : 8192).').';
    fileIn(it,:) = (file_tmp((it-1)*2+1:16:end,1) + file_tmp((it-1)*2+2:16:end)*1i).';
%     figure;plot(abs(FFTOut(i,:) - fileIn(i,:))./abs(FFTOut(i,:)))
    figure;plot(real(FFTOut(it,1:8192)));hold on;plot(real(fileIn(it,1:8192)));legend('M','C')
    figure;plot(20*log10(abs((FFTOut(it,1:8192)))));hold on;plot(20*log10(abs(fileIn(it,1:8192))));legend('M','C')
end
% figure;plot(20*log10(abs((SigRxOut(1,:)))));hold on;plot(20*log10(abs(fileIn(1,1:8192))));legend('M','C')
% figure;plot(real(SigRxOut(1,:)));hold on;plot(real(fileIn(1,1:8192)));legend('M','C')
% P7
clear filein;
fileIn = file_UD_P7((FreamNum-1)*1024+1:1024*FreamNum).';
% Tmp = fileIn(476:end);
% fileIn(475:end-1) = Tmp;
% fileIn(473:474) = P7(473:474);
figure;plot(abs(P7 - fileIn))
figure;plot(real(P7(1,:)));hold on;plot(real(fileIn(1,:)));legend('M','C')

% AfreqBinMax  AfreqBin
clear filein;
fileIn = file_UD_Out((FreamNum-1)*256+1:256*FreamNum).';
C_AfreqBinMax = fileIn(1:GWRxAllUser);
C_AfreqBin = fileIn(GWRxAllUser+1:end);
figure;plot(sort(FreqPosMax(1,:))-1);hold on;plot(sort(C_AfreqBinMax(1,:)));legend('M','C')
figure;plot(real(FreqPos(1:GWRxAllUser)));hold on;plot(real(C_AfreqBin(1,:)));legend('M','C')
