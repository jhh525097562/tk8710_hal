function [GWRxUserInfOut,GWRxUserNum,LostNum,TempUserID,ANoise_REG] = GWPHYRx(CaseNum,GW_Rx_MutiAntenna,GWRXPara,Iter,SNR)

persistent APreambleFft00ESum2;
persistent APreambleFft00;

GWSigDownSampleRx = Downsampling(GW_Rx_MutiAntenna,GWRXPara);
[SignalFloat,SignalInt_M,SignalInt_C] = fixModule(GWSigDownSampleRx,14,13,14,SNR);%12 11 12  SNR
% SignalFloat = SignalFloat.*[1 0.75 0.96 0.71 0.63 0.70 0.15 0.56].';
SigDownSampleRx = SignalFloat;
M = GWRXPara.AntennaNum; % ������
%% debug ��������
DEBUGPlot = 0;%��ÿ�ε�8���߹��ʺϳɵ�Ƶ��ͼ ==0������==1��
%%
GWRxPilotWindowNum = 1;
slideTime = GWRXPara.GWRxDataWindowNum + GWRxPilotWindowNum;     % ��������
% FftSize = SysSimulationPara.GWRxShrink*(SysSimulationPara.GWRxWindowLen*SysSimulationPara.GWRxSampleRate/125000);
FftSize = GWRXPara.GWRxFFTLen;
if Iter==1
    APreambleFft00ESum2 = ones(1,FftSize);
    APreambleFft00 =  ones(8,FftSize);
end
GWRxAllUser = GWRXPara.GWRxUserNum;

% SelectIndex(1:GWRxAllUser,1:slideTime) = 0;
SelectIndex = [];

BeamformerMatrix_REG = zeros(GWRxAllUser,M);
RxDataTemp(1:GWRxAllUser,1:1000) = 0;

DemoWindowLen = 0;
DataWindowNum(1:GWRxAllUser) = 0;
WindowAdjustLen = 0;
TrainNum = 0;
estTimingOffset_RegB_tmp(1:GWRxAllUser) = 1;
K = dec2bin(GWRXPara.AntennaWorkStatus,8);
for i=1:M
    if K(M-i+1)==dec2bin(1)
        
    else
        SigDownSampleRx(i,:) = 0;
    end
end

%% Cmodel Data Save /home/data/K2301/K2301PHY_CaseList_Data
if 0
    filename = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum),'/UD_InData',num2str(Iter-1),'.txt']; 
    RxSignalOneSlot1 = reshape(SigDownSampleRx(:,1:FftSize*slideTime),[FftSize*8*slideTime 1]);
    write2data(RxSignalOneSlot1,filename,'txt');
    if GWRXPara.FreqAllocationFlag==1
        filename1 = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum),'/UD_SetFreq.txt']; 
        TmpSetFreq = zeros(1,128);
        TmpSetFreq(1:length(GWRXPara.AllocationRXFreqSet)) = GWRXPara.AllocationRXFreqSet;
        TmpSetFreq1 = reshape(TmpSetFreq,[GWRxAllUser 1]);
        write2data(TmpSetFreq1,filename1,'txt');
    end
end
%% Cmodel Data load /home/data/K2301/K2301PHY_CaseList_Data
if 0
    filename = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum),'/UD_InData',num2str(Iter-1),'.txt']; 
    Data = dlmread(filename,'');
    RxSignalOneSlot1 = reshape(Data.',[16 FftSize*slideTime]);
    SigDownSampleRx = RxSignalOneSlot1(1:2:end,:) + RxSignalOneSlot1(2:2:end,:)*1i;
%     if GWRXPara.FreqAllocationFlag==1
%         filename1 = ['/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case',num2str(CaseNum),'/UD_SetFreq.txt']; 
%         TmpSetFreq = zeros(1,128);
%         Data1 = dlmread(filename1,'');
%         TmpSetFreq(1:length(GWRXPara.AllocationRXFreqSet)) = GWRXPara.AllocationRXFreqSet;
%         TmpSetFreq1 = reshape(TmpSetFreq,[GWRxAllUser 1]);
%         write2data(TmpSetFreq1,filename1,'txt');
%     end
end
i=0;

%%
for i=1:slideTime
    %% DataIn
    RxSignalOneSlot = zeros(M,FftSize);
    RxSignalOneSlot(1:M,1:end) = SigDownSampleRx(1:M,(WindowAdjustLen +1+(FftSize-0)*(i-1)):(WindowAdjustLen + (FftSize-0)*(i-1) + FftSize));
    %% RemoveRxADC Module
    [SigRemoveADCOut,ADC_REG] = RemoveRxADCModule(RxSignalOneSlot,M,FftSize);
    %% FFT Module
    [FFTOut(1:M,1+FftSize*(i-1):FftSize*i), ANoise_REG, APreambleFftESum1_REG] = FFTModule(SigRemoveADCOut,M,FftSize, i <= GWRxPilotWindowNum,GWRXPara,Iter);
    %% UserDection Module
    [Freq_REG, FreqPos, FreqPosMax, AH_REG, UMode, RxPilotPower_REG, APreambleFftESum2, APreambleFft,AppointTChID] = ...
        AUserDection(APreambleFft00ESum2,APreambleFft00,FFTOut,APreambleFftESum1_REG,FftSize,GWRXPara, i<=GWRxPilotWindowNum,CaseNum,Iter);
    if i<=GWRxPilotWindowNum
        if DEBUGPlot==1
            TempFreqPos = zeros(1,length(APreambleFftESum1_REG));
            TempFreqPos(FreqPosMax) = FreqPos(1:length(FreqPosMax));
            figure;
            subplot(2,1,1)
            plot(10*log10(APreambleFftESum1_REG));
            subplot(2,1,2)
            plot(TempFreqPos);
            for i1 = 1:GWRxAllUser
                text( FreqPosMax(i1),FreqPos(i1) ,'o','color','r' );
            end
        end
    end

    GWRxUserNumIn = 1;
    if i > 1 %last window decode
        Mode = UMode(1);
        [ModePara,paraRxPHY] = getmodelparaGWRxPHY2_1(Mode,GWRXPara);

        IFFTLen = paraRxPHY.ifftLen;
        DemoWindowLen = DemoWindowLen + IFFTLen;
        for j=1:GWRxAllUser
            %% GWRxBF Module
            freqOffsetChGrid = round((Freq_REG(j)+GWRXPara.GWRxSampleRate)/GWRXPara.GWRxSampleRate*FftSize);
            [IFFTOut(:,1+(i-1)*IFFTLen:i*IFFTLen)] = ...
                GWRxBFModule(i,j,FFTOut(:,1+FftSize*(i-1):FftSize*i),AH_REG(:,j),freqOffsetChGrid,M,FftSize,paraRxPHY.Mfsk,paraRxPHY.FilterLenB,...
                IFFTLen,paraRxPHY.AddZerosNum,GWRXPara.GWRXBFMode,CaseNum);
            %% GWRXBF Select Module
            [SigSelectOut(1:IFFTLen),SelectIndex(j,i),PdBTmp_REG(j,i)] = BeamFormingSelectionModule(IFFTOut(:,1+(i-1)*IFFTLen:i*IFFTLen),...
                AH_REG(:,j),M,i,ModePara.DSSS,IFFTLen,FftSize,BeamformerMatrix_REG(j,:),GWRXPara.GWRXBFMode);
            %% Decode Module
                if DemoWindowLen <= (paraRxPHY.OneBlockWindowNum*IFFTLen)
                    SigDemodIn1(j,DataWindowNum(j)*IFFTLen+1:(DataWindowNum(j)+1)*IFFTLen) = SigSelectOut;
                    DataWindowNum(j) = DataWindowNum(j) + 1;
                end
                if DemoWindowLen >= paraRxPHY.OneBlockSymbolLen
                    if j==GWRxAllUser
                        WindowAdjustLen = WindowAdjustLen + (paraRxPHY.OneBlockSymbolLen - DemoWindowLen)*(FftSize/IFFTLen);
                        DemoWindowLen = 0;
                        DataWindowNum(1:GWRxAllUser) = 0;
                    end
                    if j==1
                        TrainNum = TrainNum + 1;
%                         figure;plot(phase(SigDemodIn1(j,1:2048)));hold on;
                    end
                    RxByteLen = (ModePara.Len_TxByte-2*ModePara.crc14_en);
                    [bitErrorInFrame(j,TrainNum),FSKSNR(j,TrainNum),DQPSKSNR(j,TrainNum),RxDataTemp(j,1+(TrainNum-1)*RxByteLen:TrainNum*RxByteLen),Sbit(j,TrainNum),Fbit(j,TrainNum),estTimingOffset_RegB] = PHY2_1RxDemod(SigDemodIn1(j,:),ModePara,TrainNum,estTimingOffset_RegB_tmp(j));
                    estTimingOffset_RegB_tmp(j) = estTimingOffset_RegB;
                end
        end
    end
end

% User Info Extract
GWRxUserNumIn = 1;
for j=1:GWRxAllUser
    % User Info Extract
    if GWRxUserNumIn==1
        GWRxUserInf = [];
    end
    [GWRxUserInf,GWRxUserNum] = UserCheckModule(RxDataTemp(j,:),GWRxUserInf,GWRxUserNumIn,...
        bitErrorInFrame(j,:),FSKSNR(j,:),DQPSKSNR(j,:),Freq_REG(j),AH_REG(:,j),RxPilotPower_REG(j),AppointTChID(j),Sbit(j,:),Fbit(j,:),paraRxPHY,PdBTmp_REG(j,:),ModePara);
    GWRxUserNumIn = GWRxUserNum; 
end
%% Para Updata
APreambleFft00ESum2  = APreambleFftESum2;
APreambleFft00 = APreambleFft;
GWRxUserNum = GWRxUserNum - 1
TempFreq = [];
TempUserID = [];
GWRxUserInfOut = [];
for ii=1:GWRxUserNum
    TempUserID(ii) = GWRxUserInf(ii).UserID(1)*255+GWRxUserInf(ii).UserID(2);
    if TempUserID(ii) > GWRxAllUser || TempUserID(ii)<=0
        TempUserID(ii) = GWRxUserNum + 1;
    end
end

if GWRxUserNum>=1
    [UId,index] = sort(TempUserID);
end
LostNum = sum(sum(bitErrorInFrame,2));
for it=1:GWRxUserNum
    GWRxUserInfOut(it).SNR = GWRxUserInf(index(it)).SNR;
    GWRxUserInfOut(it).CRC = GWRxUserInf(index(it)).CRC;
    GWRxUserInfOut(it).Freq = GWRxUserInf(index(it)).Freq;
    GWRxUserInfOut(it).RxData = GWRxUserInf(index(it)).RxData;
    GWRxUserInfOut(it).RxMode = GWRxUserInf(index(it)).RxMode;
    GWRxUserInfOut(it).RxPilotPower = GWRxUserInf(index(it)).RxPilotPower;
    GWRxUserInfOut(it).RxAH = GWRxUserInf(index(it)).RxAH;
    GWRxUserInfOut(it).UserID = GWRxUserInf(index(it)).UserID;
    GWRxUserInfOut(it).AppointTChID = GWRxUserInf(index(it)).AppointTChID;
    GWRxUserInfOut(it).Sbit = GWRxUserInf(index(it)).Sbit;
    GWRxUserInfOut(it).Fbit = GWRxUserInf(index(it)).Fbit;
    GWRxUserInfOut(it).PdBTmp = GWRxUserInf(index(it)).PdBTmp;
end
% if GWRxUserNum==2
%     v1=GWRxUserInf(1).RxAH';
%     v2=GWRxUserInf(2).RxAH;
%     out = abs(v1*v2(1:8))/norm(v1)/norm(v2(1:8))
% end

end