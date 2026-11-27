
clear all
% close all

tic

% 参数设置
fs = 8;                    % 滤波前符号周期
fs2 = 8;                    % 滤波后符号周期
sampleFrequency = 125000;   % 采样率
winLen = 8192;              % 窗口长度
overlapping = 128;           % 重叠长度
filterLen = 2048;            % 滤波器长度

segment = 3;                % 滑动次数
frameLen = winLen*segment;    % 一帧的采样点数
LS = ceil(frameLen/fs);     % 仿真码元个数

frameNumber = 100;          % 帧数
SNR = [-25:1:24];             % 信噪比
% SNR = [125:1:224];             % 信噪比

% 滤波器
% h = geth(fs);
% BaseUpsamplingRateTx = 8;
% FilterRRCTx = zeros(1,6*BaseUpsamplingRateTx-1);
% tmpF = rcosdesign(0.485,6,BaseUpsamplingRateTx,'sqrt');
% figure;
% plot(tmpF);
% FilterRRCTx = tmpF(1,2:end-1)./(max(tmpF));

% FilterRRCTxTmp = [-0.00809674359268633,-0.0155781375617193,-0.0164264182467650,-0.0110680073489793,0.00212489769089282,0.0177559742579431,0.0326473576565597,0.0395653787873241,0.0326751749761926,0.0111559094213285,-0.0272327101941122,-0.0715888388774971,-0.115956507852977,-0.141971099105005,-0.138467747216662,-0.0922947191870237,0.00276390249946876,0.143004420875350,0.320327399511067,0.513865825597022,0.701834251755972,0.859105199234349,0.962610475258520,1,0.962610475258520,0.859105199234349,0.701834251755972,0.513865825597022,0.320327399511067,0.143004420875350,0.00276390249946876,-0.0922947191870237,-0.138467747216662,-0.141971099105005,-0.115956507852977,-0.0715888388774971,-0.0272327101941122,0.0111559094213285,0.0326751749761926,0.0395653787873241,0.0326473576565596,0.0177559742579431,0.00212489769089282,-0.0110680073489793,-0.0164264182467650,-0.0155781375617193,-0.00809674359268633];

% BaseUpsamplingRateRx = 8;
% FilterRRCRx = zeros(1,6*BaseUpsamplingRateRx-1);
% tmpF = rcosdesign(0.485,6,BaseUpsamplingRateRx,'sqrt');
% FilterRRCRx = tmpF(1,2:end-1)./(max(tmpF));
% 
% FilterRRCTxTmp1 = (FilterRRCTxTmp(1,floor(length(FilterRRCTxTmp)/2))*2 + FilterRRCTxTmp(1,ceil(length(FilterRRCTxTmp)/2)))/3;
% FilterRRCTxTmp2 = [FilterRRCTxTmp(1,1:floor(length(FilterRRCTxTmp)/2)) FilterRRCTxTmp1];
% FilterRRCTxTmp3 = [FilterRRCTxTmp2 fliplr(FilterRRCTxTmp2)];

BaseUpsamplingRateTx = 8;
FilterSym = 16;
FilterRRCTxTmp = zeros(1,FilterSym*BaseUpsamplingRateTx);
tmpF = rcosdesign(0.3,FilterSym,BaseUpsamplingRateTx,'sqrt');
% FilterRRCTxTmp = tmpF(1,2:end-1)./(max(tmpF));
FilterRRCTxTmp = tmpF(1,1:end)./(max(tmpF));

%     FilterRRCTxTmp1 = (FilterRRCTxTmp(1,floor(length(FilterRRCTxTmp)/2))*2 + FilterRRCTxTmp(1,ceil(length(FilterRRCTxTmp)/2)))/3;
FilterRRCTxTmp1 = 1;
FilterRRCTxTmp2 = [FilterRRCTxTmp(1,1:floor(length(FilterRRCTxTmp)/2)) FilterRRCTxTmp1];
FilterRRCTxTmp3 = [FilterRRCTxTmp2 fliplr(FilterRRCTxTmp2)];

h = FilterRRCTxTmp3;
% figure;
% plot(h);

% HJH ADD 20220627
% FilterRRCTx = [-0.0009765625,-0.0029296875,-0.0029296875,0.0009765625,0.0048828125,0.00390625,-0.001953125,-0.0068359375,-0.005859375,0.0009765625,0.0078125,0.0087890625,0.001953125,-0.0078125,-0.009765625,-0.0009765625,0.0107421875,0.0126953125,-0.001953125,-0.0234375,-0.0302734375,-0.005859375,0.041015625,0.07421875,0.0517578125,-0.03515625,-0.1416015625,-0.1796875,-0.0693359375,0.2060546875,0.568359375,0.8779296875,1,0.8779296875,0.568359375,0.2060546875,-0.0693359375,-0.1796875,-0.1416015625,-0.03515625,0.0517578125,0.07421875,0.041015625,-0.005859375,-0.0302734375,-0.0234375,-0.001953125,0.0126953125,0.0107421875,-0.0009765625,-0.009765625,-0.0078125,0.001953125,0.0087890625,0.0078125,0.0009765625,-0.005859375,-0.0068359375,-0.001953125,0.00390625,0.0048828125,0.0009765625,-0.0029296875,-0.0029296875,-0.0009765625];
% h = FilterRRCTx;

SNRCheck = zeros(frameNumber,length(SNR));
ECount = zeros(frameNumber,length(SNR));
SNRTable = zeros(length(SNR),3);
SNRTable(:,1) = SNR';

for ii = 1:frameNumber
    
    % 生成符号
%     symbols = randi([0 1],LS,1);
%     symbols(symbols==0) = -1;
%     info = symbols;
%     info(2:2:end,:) = info(2:2:end,:)*(1i);
s = RandStream('mt19937ar','Seed',ii);
RandStream.setGlobalStream(s);

    ModOrder = 3;
    symbolsbit = randi([0 1],1,LS*ModOrder);

    tmpM = zeros(1,LS);
    for i1=1:ModOrder
        tmpM = tmpM + 2^(ModOrder-i1)*symbolsbit(1,i1:ModOrder:end);
    end
    info(1,:) = pskmod(tmpM,2^ModOrder,0,'gray');
    
    % HJH ADD 20220627
%     Mod8Psk = comm.PSKModulator('ModulationOrder',8,'PhaseOffset',0,'BitInput',true,'SymbolMapping','Gray');
%     Mod8PSKSymbol = step(Mod8Psk,intrlvBits');
    
    infoup = upsample(info,fs);
    
    temp = conv(infoup,h);
    modSignal = temp(ceil(length(h)/2):end-floor(length(h)/2));
    PSK = transpose(modSignal(:));
    
%     figure;
%     plot(abs(PSK));
    
    % MSK信号滤波
    PSK1 = channelize20180408A(PSK, 0, overlapping, winLen, filterLen, sampleFrequency);
    PSK2 = channelize20180408C(PSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2;
%     PSK2 = channelize20180408C(PSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1);
%     PSK2 = channelize20180408C(PSK1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1);
%     figure;
%     plot(abs(PSK2));
    
    PSKPower = PSK2*PSK2';
    
    for jj = 1:length(SNR)
        
        % 生成噪声
        sigma = sqrt(1/(10^(SNR(jj)/10)));
        noise = sigma*(randn(1,length(PSK))+(1i)*randn(1,length(PSK)))/sqrt(2);
        
        % 噪声滤波
        noise1 = channelize20180408A(noise, 0, overlapping, winLen, filterLen, sampleFrequency);
%         noise2 = channelize20180408C(noise1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
        noise2 = channelize20180408C(noise1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2;
        noisePower = noise2*noise2';
        
        SNRHat = 10*log10(PSKPower/noisePower);
        SNRCheck(ii,jj) = SNRHat;
        
%         % 检测滤波前相加和滤波后相加是否相同，相同
%         dataA = MSK + noise;
%         dataA1 = channelize20180408A(dataA, 0, overlapping, winLen, filterLen, sampleFrequency);
%         dataA2 = channelize20180408C(dataA1, 0, filterLen, fs, fs2, overlapping, winLen, sampleFrequency, frameLen,1)/2/0.9081;
%         dataB2 = MSK2 + noise2;
%         plot(real(dataA2-dataB2));
        
        % 统计
%         load('userData1.mat');
%         data = userData(1,8064+1:8064*2);
%         data = userData(1,:);
%         data = PSK;
        data = PSK2 + noise2;
%         data = PSK2;
%         data = PSK2(1:2:end) + noise2(1:2:end);
        dataAbs = abs(data);
        P = dataAbs*dataAbs'/length(dataAbs);
        dataNormal = dataAbs/sqrt(P);
        E = mean(dataNormal);
        dataAbs0 = abs(data).^2;
        p0 = sum(dataAbs0)/length(dataAbs0);
%         dataNormal0 = dataAbs0/sqrt(p0);
        dataNormal0 = (dataAbs0./p0).^2;
        E0 = mean(dataNormal0);
        ECount(ii,jj) = E;
        ECount0(ii,jj) = E;
    end
ii
end

SNRTable(:,2) = mean(SNRCheck);
% SNRTable(:,3) = mean(ECount);
SNRTable(:,3) = mean(ECount0);
%% HJH ADD 20220628
SNRTable1(:,3) = SNRTable(:,3)*0.9750/0.9585;
SNRTable2=SNRTable;
SNRTable2(51:50+24,3) = SNRTable1(27:50,3);

TestTable(1:50,:) = SNRTable2(1:50,:);
for i=1:25
% TestTable(25+i,3) = TestTable(25+i,3) + i*(0.9750-0.9585)/25;
TestTable(25+i,3) = TestTable(25+i,3) + i*(1.2609-1.3078)/25;
end

for i=1:24
SNRTable2(50+i,1) = SNRTable2(49+i,1)+1;
SNRTable2(50+i,2) = SNRTable2(49+i,2)+1;
end
% save('D8PSKTable20220628.mat','SNRTable2');
save('D8PSKTable20221019.mat','TestTable');
%%
PSKTable = SNRTable;
save('PSKTable.mat','PSKTable');


toc



















