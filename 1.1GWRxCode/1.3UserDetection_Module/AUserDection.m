function [AFreqOffsetCh_o,AFreqBinD_o,AFreqBinDMax_o,AH_o,UMode_o,rxPilotPower_o,APreambleFftESum1_o,APreambleFft1_o,AppointTChID_o] = AUserDection(APreambleFftESum0,APreambleFft0,FFTOut1,APreambleFftESum1Tmp,FftSize,GWRXPara, flag_en,CaseNum,Iter)
persistent AFreqOffsetCh AFreqBinD AFreqBinDMax AH UMode rxPilotPower APreambleFftESum1 APreambleFft1 AppointTChID
global MaxOutLen;
MaxOutLen = 1024;

if(flag_en)
    FFTOut1 = [FFTOut1(:,FftSize/2+1:FftSize) FFTOut1(:,1:FftSize/2)];

    %%
    %�������ʣ��ܹ������û�����
    GWRxAllUser = sum(GWRXPara.GWRxUserNum);
    AppointTChID(1:GWRxAllUser) = -1;
    AFreqBinDMax(1:GWRxAllUser) = 1;
    UMode(1:GWRxAllUser) = GWRXPara.RxMode(1);
    AH(1:GWRXPara.AntennaNum,1:GWRxAllUser) = 0;
    TempFFTLen = FftSize;
    AFreqBinD = zeros(1,MaxOutLen);
    P1 = zeros(1,MaxOutLen);P2 = zeros(1,MaxOutLen);P3 = zeros(1,MaxOutLen);P4 = zeros(1,MaxOutLen);
    P5 = zeros(1,MaxOutLen);P6 = zeros(1,MaxOutLen);P7 = zeros(1,MaxOutLen);
    APreambleFft1 = FFTOut1(:,1:TempFFTLen);
    APreambleFftESum1 = APreambleFftESum1Tmp;

    APreambleFftESum2 = APreambleFftESum1;
    APreambleFftESum3 =  APreambleFftESum0;
    APreambleFft2 = APreambleFft1;
    APreambleFft3 = APreambleFft0;
    if GWRXPara.FreqAllocationFlag==0%Ƶ��δ��ǰ����
        %����ͻ������������ź����ڣ������ڵ���ȣ���ͻ��10dB���ϣ������ܣ�
        [P1,HighLightPos,Idx] = P1_Rule(APreambleFftESum2,TempFFTLen,GWRXPara);
        %����ʲ�����(������ʵ�45dB������)
        [P2,HighLightPos] = P2_Rule(APreambleFftESum2,HighLightPos,P1,TempFFTLen,GWRXPara);
        %����(�Ⱦ����20dB�����ܡ��뾵������Ըߣ������Ե�)
        P3 = P3_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P2,TempFFTLen,GWRXPara);
        %����һʱ϶��ȣ�ͬһƵ�㼰���ڵ㹦�ʽӽ�������Ըߡ������Ե�
        P4 = P4_Rule(APreambleFftESum2,APreambleFftESum3,APreambleFft2,APreambleFft3,HighLightPos,P3,TempFFTLen);
        %�����ڵ���ȣ����ʵ͡�����Ըߡ������Ե�
        P5 = P5_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P4,TempFFTLen);
        %��Ƶ����128���ڣ��б������10dB������Ըߡ������Ե�
        P6 = P6_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P5,TempFFTLen,GWRXPara);
        %���ڸ��ţ��������������������T��Ƶ����ȡ����ʽӽ�������Ըߡ������Ե�
        P7 = P7_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P6,Idx,TempFFTLen);
        AFreqBinD = P7;


        %�û�ģʽ�жϺ��ŵ���Ϣ��ȡ
        [Ctmp,ITmp] = sort(AFreqBinD,'descend');
        AFreqBinDMax(1:GWRxAllUser) = HighLightPos(ITmp(1:GWRxAllUser));
        AFreqBinD(1:GWRxAllUser) = Ctmp(1:GWRxAllUser);
    elseif GWRXPara.FreqAllocationFlag==1%��Ƶ����ǰ�����ն˽���
         %����ͻ������������ź����ڣ������ڵ���ȣ���ͻ��10dB���ϣ������ܣ�
        [P1,HighLightPos,Idx] = P1_Rule(APreambleFftESum2,TempFFTLen,GWRXPara);

        for i=1:GWRXPara.AllcationUserNum
            FreqPos(i) = ceil(GWRXPara.AllocationRXFreqSet(i)*TempFFTLen/GWRXPara.GWRxSampleRate);
            FreqPos(i) = FreqPos(i) + TempFFTLen/2+1;
        end
        for i=1:GWRXPara.AllcationUserNum
            FreqPosIndex0(i) = ceil(FreqPos(i)-200*TempFFTLen/GWRXPara.GWRxSampleRate);
            FreqPosIndex1(i) = ceil(FreqPos(i)+200*TempFFTLen/GWRXPara.GWRxSampleRate);
            [Value(i),TempFreqBinD(i)] = max(APreambleFftESum2(FreqPosIndex0(i):FreqPosIndex1(i)));
        end
        AFreqBinD0 = FreqPosIndex0 + TempFreqBinD - 1;
        %�û�ģʽ�жϺ��ŵ���Ϣ��ȡ
        RealFreqAllocationNum=0;
        for i=1:GWRXPara.AllcationUserNum
%             if(P1(find(HighLightPos==AFreqBinD0(i)))==1)
%             if(APreambleFftESum2(AFreqBinD0(i)) > (mean(APreambleFftESum2(AFreqBinD0(i)-8:AFreqBinD0(i)-3)))*7 )
                RealFreqAllocationNum = RealFreqAllocationNum+1;
                AFreqBinDMax(RealFreqAllocationNum) = AFreqBinD0(i);
                UMode(RealFreqAllocationNum) = GWRXPara.AllocationRXModeSet(i);
%             else
%                 
%             end
        end
        AFreqBinD(1:RealFreqAllocationNum) = 1;
        %ʶ����Ƶ����ǰ�����ն˺��ж��Ƿ����������������û���Ҫʶ��
        if (GWRxAllUser - RealFreqAllocationNum) > 0%��ȥ������ǰ�����ն˺����������ն���Ҫ����
            %             %��Ƶ����ǰ�����ն˵��źŴӹ���ͼ��ȥ��
            %             TempAFreqBinDMin = AFreqBinDMax(1:UserNum)-5;
            %             TempAFreqBinDMax = AFreqBinDMax(1:UserNum)+5;
            %             for k=1:UserNum
            %                 APreambleFftESum2(TempAFreqBinDMin(k):TempAFreqBinDMax(k)) = 0;
            %             end
            %����ͻ������������ź����ڣ������ڵ���ȣ���ͻ��10dB���ϣ������ܣ�
%             [P1,HighLightPos,Idx] = P1_Rule(APreambleFftESum2,TempFFTLen,SysSimulationPara);
            %����ʲ�����(������ʵ�45dB������)
            [P2,HighLightPos] = P2_Rule(APreambleFftESum2,HighLightPos,P1,TempFFTLen,GWRXPara);
            %         length(HighLightPos)
            %����(�Ⱦ����20dB�����ܡ��뾵������Ըߣ������Ե�)
            P3 = P3_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P2,TempFFTLen,GWRXPara);
            %����һʱ϶��ȣ�ͬһƵ�㼰���ڵ㹦�ʽӽ�������Ըߡ������Ե�
            P4 = P4_Rule(APreambleFftESum2,APreambleFftESum3,APreambleFft2,APreambleFft3,HighLightPos,P3,TempFFTLen);
            %�����ڵ���ȣ����ʵ͡�����Ըߡ������Ե�
            P5 = P5_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P4,TempFFTLen);
            %��Ƶ����128���ڣ��б������10dB������Ըߡ������Ե�
            P6 = P6_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P5,TempFFTLen,GWRXPara);
            %���ڸ��ţ��������������������T��Ƶ����ȡ����ʽӽ�������Ըߡ������Ե�
            P7 = P7_Rule(APreambleFftESum2,APreambleFft2,HighLightPos,P6,Idx,TempFFTLen);
            
            AFreqBinD(RealFreqAllocationNum+1:MaxOutLen) = P7(1:(MaxOutLen-RealFreqAllocationNum));
            
            %��ʶ����û��жԴ�����ǰ�����ն˵�������û�����ȥ��
            TempAFreqBinDMin = AFreqBinDMax(1)-64;%125k�����ʡ�8192FFT��64��Ӧ1Khz����
            TempAFreqBinDMax = AFreqBinDMax(RealFreqAllocationNum)+64;
            for i=1:MaxOutLen
                if HighLightPos(i)>=TempAFreqBinDMin && HighLightPos(i)<=TempAFreqBinDMax
                    AFreqBinD(i) = 0;
                end
            end
            %�û�ģʽ�жϺ��ŵ���Ϣ��ȡ
            UserNum = GWRxAllUser - RealFreqAllocationNum;
            [Ctmp,ITmp] = sort(AFreqBinD,'descend');
            AFreqBinDMax(RealFreqAllocationNum+1:GWRxAllUser) = HighLightPos(ITmp(1:UserNum));
            AFreqBinD(1:RealFreqAllocationNum) = 1;
            AFreqBinD(RealFreqAllocationNum+1:GWRxAllUser) = Ctmp(1:UserNum);
        end
    else

    end
%     AFreqBinDMax = sort(AFreqBinDMax);
%     [C_AfreqBinMax,C_AfreqBin] = UD_Compare(AFreqBinDMax,AFreqBinD,GWRxAllUser,CaseNum,Iter);
%     AFreqBinDMax = C_AfreqBinMax+1;
    %�û�ģʽ�жϺ��ŵ���Ϣ��ȡ
    for Ai8 = 1:GWRxAllUser
        PreambleNTemp(1,Ai8) = AFreqBinDMax(1,Ai8)-TempFFTLen/2-1;
        APreambleNFreq(1,Ai8) = PreambleNTemp(1,Ai8)/TempFFTLen*GWRXPara.GWRxSampleRate;
        AH(:,Ai8) = APreambleFft1(:,AFreqBinDMax(1,Ai8)); % �ŵ���Ӧ
    end
    
    AFreqOffsetCh = APreambleNFreq;
    % AFreqOffsetCh = APreambleNFreq+976.6;%FPGA�ն�����Ƶ
    %ʶ����Ƶ����ǰ�����ն˺��ж��Ƿ����������������û���Ҫʶ��
    if GWRxAllUser - GWRXPara.AllcationUserNum > 0
        for i=(GWRXPara.AllcationUserNum+1):GWRxAllUser
            UMode(i) = GWRXPara.RxMode(1);
        end
    end
    rxPilotPower = APreambleFftESum1(1,AFreqBinDMax(1:GWRxAllUser));
end
AFreqOffsetCh_o = AFreqOffsetCh;
AFreqBinD_o = AFreqBinD;
AFreqBinDMax_o = AFreqBinDMax;
AH_o = AH;
UMode_o = UMode;
rxPilotPower_o = rxPilotPower;
APreambleFftESum1_o =  APreambleFftESum1;
APreambleFft1_o = APreambleFft1;
AppointTChID_o = AppointTChID;
function [APreambleNFreq,AH,UMode,AppointTChID] = MAC2PHYRx(APreambleNFreq,AH,UMode)
    AppointAHUserNum = 1;%�ŵ�ָ�����û�����
    AppointTChID(1:AppointAHUserNum) = 1:AppointAHUserNum;%ÿ���û���TChID
    APreambleNFreq(end-AppointAHUserNum+1:end) = 0;%ÿ���û���Ƶ��
    AH(:,end-AppointAHUserNum+1:end) = 1;
    UMode(end-AppointAHUserNum+1:end) = 1;
end
end