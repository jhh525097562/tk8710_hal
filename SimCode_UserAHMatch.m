function [H_DownLink,PathAttenuation,GWTXMode,GWTXPower,GWTXAH,GWTxData,Fbits,Sbits,GWTxFreq,TMRxFreq,GWRxPilotPower] = SimCode_UserAHMatch(DownLinkSimSwitch,H,GWAntennaRxPhaseNoReciprocal,PathAttenuationIn,TMTxPower,TMTxFreq,TMRxMode,GWRxUserNum,TempUserID,GWRxUserInf,TMTXPara)

TerminalTxNum = TMTXPara.TxUserNum;

if DownLinkSimSwitch==1

    GWRxIndex = sort(TempUserID,'ascend');
    GWRxIndex(find(GWRxIndex>TerminalTxNum)) = TerminalTxNum;
    H_DownLink = H(:,GWRxIndex(1:GWRxUserNum)).';
    H_DownLink1 = H_DownLink.'.*GWAntennaRxPhaseNoReciprocal;
    GWTXMode = TMRxMode(GWRxIndex(1:GWRxUserNum));
    GWTXPower = TMTxPower(GWRxIndex(1:GWRxUserNum));
    PathAttenuation = PathAttenuationIn(GWRxIndex(1:GWRxUserNum));
    for ii = 1:GWRxUserNum
        GWTxData(ii,1:2) = GWRxUserInf(ii).UserID(1:2);
        GWTxData(ii,3:300) = floor(rand(1,300-2)*255);
        Fbits(ii,:) = zeros(1,16);
        Fbits(ii,1)= 1;
        Sbits(ii,:) = ones(1,16);
        Sbits(ii,end) = 0;
        GWTxFreq(ii) = GWRxUserInf(ii).Freq;
        GWTXAH(:,ii) = GWRxUserInf(ii).RxAH;
        GWRxPilotPower(ii) = GWRxUserInf(ii).RxPilotPower;
    end
    TMRxFreq = TMTxFreq(GWRxIndex(1:GWRxUserNum));
elseif DownLinkSimSwitch==2
    GWRxUserNum = 1;
    GWTXPower = 50;
    PathAttenuation = 0;
    for ii = 1:GWRxUserNum
        GWTxData(ii,1:2) = GWRxUserInf(ii).UserID(1:2);
        GWTxData(ii,3:300) = floor(rand(1,300-2)*255);
        GWTxFreq(ii) =0;
        TMRxFreq(ii) = GWTxFreq(ii);
        GWTXMode(ii) = 13;
        %                 H_DownLink(ii,:) = H(:,ii).';
        H_DownLink(ii,:) = ones(1,8);
    end
%     BeamformerMatrix = zeros(1,8);
%     BeamformerMatrix(1) = 1;
%     BeamformerMatrix = BeamformerMatrix.*sqrt(GWTXPower);
else
    
end


end