function  [BeamformerMatrix,BFUserNum] =  GW_Rx_BeamformingMatrix(Hhat, GWRxMode , Freq ,GWRxAllUser,GWRXPara,GWRXBFMode)

    %% Normalize Hhat
    M = size(Hhat,1);
    for i = 1:GWRxAllUser
        HNormal(:,i) = Hhat(:,i)/sqrt(sum(abs(Hhat(:,i).^2))); %归一化当前信号的信道响应
    end
    BFUserNum = [];
    BeamformerMatrix = [];
    %% Gen Beamformer
    % BW_overlop_w = 1.25;
    BW_overlop_w = 1.0;
    % MMSE = 0.02;
    MMSE = 0.00;
    
    HNormal1 = HNormal;
    
    for i = 1:GWRxAllUser
        if GWRXBFMode(i)==1
            BW1 = GetRxModeBW(GWRxMode(i));
            NumUserBF = 1;
            UserIndex = 1;
            BFNumIndex(1:GWRxAllUser) = 0;
            for j=1:GWRxAllUser
                BW2 = GetRxModeBW(GWRxMode(j));
                if (abs(Freq(i) - Freq(j)) < BW_overlop_w*(BW1+BW2)/2) || i==j
                    BFNumIndex(NumUserBF) = j;
                    if i==j
                        UserIndex = NumUserBF;
                    end
                    NumUserBF = NumUserBF + 1;
                end
            end
            %     NumUserBF = 2;
            if (NumUserBF-1)==1%%CB
                CBBeamformer = HNormal1(:,UserIndex)';
                BeamformerMatrix(i,:) = CBBeamformer;
            else%%MMSE ZF
                for ij=1:NumUserBF-1
                    HNormalTemp(:,ij) = HNormal1(:,BFNumIndex(ij));
                end
                MMSEBeamformer = HNormalTemp'* pinv(HNormalTemp*HNormalTemp'+ MMSE*( eye(M) ) );
                BeamformerMatrix(i,:) = MMSEBeamformer(UserIndex,:);
            end
            BFUserNum(i) =  NumUserBF-1;
            BeamformerMatrix(i,:) = BeamformerMatrix(i,:)/sqrt(sum(abs(BeamformerMatrix(i,:).^2)));
        else
            BeamformerMatrix(i,1:M) = 1;
        end
    end
end
function [BW] = GetRxModeBW(RxMode)
    switch RxMode
        case 1
            BW = 2000;
        case 2
            BW = 2000;
        case 3
            BW = 2000;
        case 4
            BW = 4000;
        case 5
            BW = 8000;
        case 6
            BW = 16000;
        case 7
            BW = 32000;
        case 8
            BW = 64000;
        case 9   
            BW = 128000;
        case 10
            BW = 128000;
    end
end