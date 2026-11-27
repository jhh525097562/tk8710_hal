function [SNR,G_Dif,C_G,C_V,C_BFOut] = SVD_Compare(R,G,V,UserIndex,SlideIndex,CaseNum)

global file_SVDIn_R;
global file_SVDOut_G;
global file_SVDOut_V;
global file_BFModuleOut;
% CaseNum = 97;
AntennaNum = 8;
if UserIndex==1 && SlideIndex==2
    filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/SVDIn_R.dat'];
    file_SVDIn_R = load(filename);
    filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/SVDOut_G.dat'];
    file_SVDOut_G = load(filename);
    filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/SVDOut_V.dat'];
    file_SVDOut_V = load(filename);
    filename = ['/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case',num2str(CaseNum),'/BFModuleOut.dat'];
    file_BFModuleOut = load(filename);
end
% UserNum = (SlideIndex-2)*128 + UserIndex - 1;
UserNum = (SlideIndex-2)*10 + UserIndex - 1;
startPos1 = UserNum*64;
startPos2 = UserNum*128;
startPos3 = UserNum*1024*2*8;
file_o_in_T2 = file_SVDIn_R(startPos2+1:startPos2+64*2);
file_o_in_T3 = file_SVDOut_G(startPos1+1:startPos1+64);
file_o_in_T4 = file_SVDOut_V(startPos2+1:startPos2+64*2);
file_o_in_T5 = file_BFModuleOut(startPos3+1:startPos3+1024*2*8);
for i=1:AntennaNum
    C_R(i,:) = ((file_o_in_T2((i-1)*2+1:16:end,1) + file_o_in_T2((i-1)*2+2:16:end)*1i).');
    C_BFOut(i,:) = ((file_o_in_T5((i-1)*2*1024+1:2:i*2*1024,1) + file_o_in_T5((i-1)*2*1024+2:2:i*2*1024)*1i).');
%     figure;plot(abs(R(i,:) - C_R(i,:))./abs(R(i,:)))
%     figure;plot(real(R(i,:)));hold on;plot(real(C_R(i,:)));legend('M','C')
end
for i=1:AntennaNum
    C_G(i,:) = ((file_o_in_T3((i-1)*8+1:8*i,1) + 0*1i).');
end
G_Dif = abs(diag(G) - diag(real(C_G)))./abs(diag(real(C_G)));
% figure;plot(abs(diag(G) - diag(real(C_G))))
% figure;plot(real(diag(G)));hold on;plot(real(diag(C_G)));legend('M','C')

for i=1:AntennaNum
    C_V(i,:) = ((file_o_in_T4((i-1)*2+1:16:end,1) + file_o_in_T4((i-1)*2+2:16:end)*1i).');
%     figure;plot(abs(V(i,:) - C_V(i,i)).^2./abs(V(i,:)).^2)
%     figure;plot(real(V(i,:)));hold on;plot(real(C_V(i,:)));legend('M','C')
end
SNR = [snr_lineva(V(:, 1), C_V(:, 1),0) ...
snr_lineva(V(:, 2), C_V(:, 2),0) ...
snr_lineva(V(:, 3), C_V(:, 3),0) ...
snr_lineva(V(:, 4), C_V(:, 4),0) ...
snr_lineva(V(:, 5), C_V(:, 5),0) ...
snr_lineva(V(:, 6), C_V(:, 6),0) ...
snr_lineva(V(:, 7), C_V(:, 7),0) ...
snr_lineva(V(:, 8), C_V(:, 8),0)];

end

