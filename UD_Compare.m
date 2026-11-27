function [C_AfreqBinMax,C_AfreqBin] = UD_Compare(AFreqBinDMax,AFreqBinD,GWRxAllUser,CaseNum,Iter)



% CaseNum = 97;
AntennaNum = 8;
FreamNum = Iter;

filename = ['/home/data/K2301/K2301PHY_CaseList_Data/UDModule_Data/case',num2str(CaseNum),'/UD_Out.dat'];
file_UD_Out = load(filename);

% AfreqBinMax  AfreqBin
clear filein;
fileIn = file_UD_Out((FreamNum-1)*GWRxAllUser*2+1:GWRxAllUser*2*FreamNum).';
C_AfreqBinMax = fileIn(1:GWRxAllUser);
C_AfreqBin = fileIn(GWRxAllUser+1:end);
figure;plot(sort(AFreqBinDMax(1,:))-1);hold on;plot(sort(C_AfreqBinMax(1,:)));legend('M','C')
figure;plot(real(AFreqBinD(1:GWRxAllUser)));hold on;plot(real(C_AfreqBin(1,:)));legend('M','C')





end

