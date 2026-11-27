function  [BeamformerMatrix,PerPower1] =  GW_Tx_Beamforming(GWRXAH,GWRxPilotPower_REG,GWTxMode,GWTXFreq,DownlinkUserNum,GWTXPara,GWTxDataPower,GWRxANoise_REG,CalibrateFactor_REG)
for ii = 1:DownlinkUserNum
    if GWTXPara.GWTxAHSelectFlag==0
        GWTxAH(:,ii) = GWRXAH(:,ii).*sqrt(GWRxANoise_REG);
    else
%         GWTxAH(:,ii) = H_DownLink(:,ii);
    end
    % �ŵ���У׼����
    if GWTXPara.RecipracalCaliFlag==1%Uplink to Downlink channel calibrate
        Hhat(:,ii) = GWTxAH(:,ii)./(CalibrateFactor_REG.');
    else
        Hhat(:,ii) = GWTxAH(:,ii);
    end
end
%% Normalize Hhat
M = size(Hhat,1);
for i=1:M
    if abs(CalibrateFactor_REG(i))==0
        Hhat(i,:) = 0;
    end
end
for i = 1:DownlinkUserNum
    HNormal(:,i) = Hhat(:,i)/sqrt(sum(abs(Hhat(:,i)).^2)); %��һ����ǰ�źŵ��ŵ���Ӧ
end

%% Gen Beamformer
% BW_overlop_w = 1.25;
BW_overlop_w_REG = 1.0;
% MMSE = 0.02;
MMSE_REG = 0.00;
MMSEBeamformer_tmp = [];
pinv_new_in = [];
pinv_out = [];
for i = 1:DownlinkUserNum
    BW1 = GetTxModeBW(GWTxMode(i));
    BW(i) = BW1;
    NumUserBF = 1;
    UserIndex = 1;
    BFNumIndex(1:DownlinkUserNum) = 0;
    for j=1:DownlinkUserNum

        BW2 = GetTxModeBW(GWTxMode(j));

        if abs(GWTXFreq(i) - GWTXFreq(j)) < BW_overlop_w_REG*(BW1+BW2)/2
            BFNumIndex(NumUserBF) = j;
            if i==j
                UserIndex = NumUserBF;
            end
            NumUserBF = NumUserBF + 1;
%             fprintf('%d, %d\n', i-1, j-1);
%             disp(NumUserBF);
        end
    end
    
    if (NumUserBF-1)==1%%CB
        CBBeamformer = HNormal(:,UserIndex)';
        BeamformerMatrix(i,:) = CBBeamformer;
    else%%MMSE ZF
        HNormalTemp = [];
        for ij=1:NumUserBF-1
            HNormalTemp(:,ij) = HNormal(:,BFNumIndex(ij));
        end
        pinv_in_mat = HNormalTemp*HNormalTemp'+ MMSE_REG*( eye(M) ) ;
        pinv_out_mat = pinv(pinv_in_mat);
%         pinv_out_mat = pinv_new(pinv_in_mat);
%         [pinv_out_mat] = Pinv_Matlab_C(pinv_in_mat);
        if find(isnan(pinv_out_mat))>0
            0;
        end
        MMSEBeamformer = HNormalTemp'* pinv_out_mat;
        pinv_new_in = [pinv_new_in reshape(transpose(pinv_in_mat), 1, [])];
        pinv_out = [pinv_out reshape(pinv_out_mat, 1, [])];
        MMSEBeamformer_tmp = [MMSEBeamformer_tmp reshape(transpose(MMSEBeamformer), 1, [])];

%         MMSEBeamformer = HNormalTemp'* pinv(HNormalTemp*HNormalTemp'+ MMSE_REG*( eye(M) )  );
        BeamformerMatrix(i,:) = MMSEBeamformer(UserIndex,:);
    end
    BeamformerMatrix(i,:) = BeamformerMatrix(i,:)/sqrt(sum(abs(BeamformerMatrix(i,:).^2)));
end

%% Power Allocate
ToTalPower_REG = 50*3;%50*3
if GWTXPara.GWTxPowerFlag==0%No power limit
    ToTalPower_REG = sum(GWTxDataPower(1:DownlinkUserNum));
    PowerPerUser(1:DownlinkUserNum) = GWTxDataPower(1:DownlinkUserNum);
%     PowerPerUser(1:DownlinkUserNum) = ToTalPower;
elseif GWTXPara.GWTxPowerFlag==1%Power limit AveragePowerAllocation
    PowerPerUser(1:DownlinkUserNum) = ToTalPower_REG/DownlinkUserNum;
elseif GWTXPara.GWTxPowerFlag==2%Power limit GWRxPowerAllocation
    % ����BF��AH�����е�Ƶ����������
    SNR_REG(1:DownlinkUserNum)=1;
    G_RGE(1:DownlinkUserNum)=1;
    PowerDiff1_REG = 10;
    PowerDiff2_REG = 15;
    PowerDiff3_REG = 20;
    [PowerPerUser] = PowerAllocation(ToTalPower_REG,GWRxPilotPower_REG,BeamformerMatrix,HNormal,GWTXFreq,BW,SNR_REG,G_RGE,DownlinkUserNum,PowerDiff1_REG,PowerDiff2_REG,PowerDiff3_REG);
else
end

PerPower = PowerPerUser./(sum(PowerPerUser));
PerPower1 = pow2db(PerPower*ToTalPower_REG);

for i = 1:DownlinkUserNum
    BeamformerMatrix(i,:) = BeamformerMatrix(i,:).*sqrt(PowerPerUser(i));
end

end
function [BW] = GetTxModeBW(TxMode)
    switch TxMode
        case 11
            BW = 2000;
        case 12
            BW = 2000;
        case 13
            BW = 2000;
        case 14
            BW = 4000;
        case 15
            BW = 8000;
        case 16
            BW = 16000;
        case 17
            BW = 32000;
        case 18
            BW = 64000;
        case 19   
            BW = 128000;
        case 20
            BW = 128000;
    end
end