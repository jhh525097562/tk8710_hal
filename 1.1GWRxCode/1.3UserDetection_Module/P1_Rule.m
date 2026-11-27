function [P1_o,pos_o,PeriodicIdx] = P1_Rule(APreambleFftESum1,TempFFTLen,GWRXPara)

K = db2pow(GWRXPara.GWRxP1_The);
%% 方式1：固定进行128-2次噪底计算，shrink=1，128个频点计算噪底为标准，shrink调整后，计算噪底的频点个数同步降低
% TempLen = 64*SysSimulationPara.GWRxShrink;
% Tempstart1 = 96*SysSimulationPara.GWRxShrink;
% Tempstart2 = 224*SysSimulationPara.GWRxShrink;
% Tdiff =  Tempstart2 - Tempstart1;
% i = ((0:125)*TempLen+Tempstart1);
% j = ((0:125)*TempLen+Tempstart2);
% index1 = [TempLen+1;TempLen+1+Tdiff];
% index2 = [i;j];
% index = [index1 index2];
% m = size(index,2);
% The = zeros(1,TempFFTLen);
% for i = 1:m-1
%     THE = getThreadValue(APreambleFftESum1(index(1,i):index(2,i)),SysSimulationPara);
%     if i == 1
%         The(Tdiff+1:160*SysSimulationPara.GWRxShrink) = THE;
%     else
%         The(i*TempLen+TempLen/2+1:(i+1)*TempLen+TempLen/2) = THE;
%     end
% end
%% 方式2：根据FFT长度，分了三档计算噪底
% if TempFFTLen==1024
%     Tdiff = 32;
% elseif TempFFTLen==2048 || TempFFTLen==4096
%     Tdiff = 64;
% elseif TempFFTLen==8192
%     Tdiff = 128;
% end
% index = [Tdiff/2:Tdiff:TempFFTLen];
% m = size(index,2);
% The = zeros(1,TempFFTLen);
% for i = 1:m-1
%     THE = getThreadValue(APreambleFftESum1(index(1,i):(index(1,i)+Tdiff)),SysSimulationPara);
%     The(index(1,i):(index(1,i)+Tdiff)) = THE;
% end
%% 方式3，
if TempFFTLen<=2048
    Tdiff = 64;
elseif TempFFTLen>2048 && TempFFTLen<=8192
    Tdiff = 128;
else
    Tdiff = 256;    
end
index1 = [Tdiff/2:Tdiff/2:TempFFTLen];
m = size(index1,2);
The = zeros(1,TempFFTLen);
for i = 1:m-2
    THE = getThreadValue(APreambleFftESum1(index1(1,i)+1:(index1(1,i)+Tdiff)),GWRXPara);
    The(index1(1,i)+1:(index1(1,i+1)+Tdiff/2)) = THE;
end

global MaxOutLen;

pos_o = ones(1,MaxOutLen);
P1_o = zeros(1,MaxOutLen);

P1 = zeros(1,TempFFTLen);
P1 = double((APreambleFftESum1 > The*K));
P1(TempFFTLen-Tdiff+1:end) = 0;
P1(1:Tdiff) = 0;
pos = find(P1==1);
if length(pos)>=MaxOutLen
    pos_o = pos(1:MaxOutLen);
    P1_o(1:MaxOutLen) = 1;
else
    pos_o(1:length(pos)) = pos;
    P1_o(1:length(pos)) = 1;
end

% save P1_15 P1;
% save APreambleFftESum1_15 APreambleFftESum1;
% PeriodicIdx = InterfrencePeriodicIdxCalcu(P1,TempFFTLen);
PeriodicIdx = InterfrencePeriodicIdxCalcuNew(P1,TempFFTLen);

% figure;plot(10*log10(APreambleFftESum1));hold on;plot(10*log10(The));hold on;plot(10*log10(The*K))
end


