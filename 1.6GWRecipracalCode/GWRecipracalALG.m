function [CalibrateFactor] = GWRecipracalALG(UpLinkH,DownLinkH,GainDif)

global P;
global NumCarrier;

% GainDif = sqrt(db2pow(DataTxGain))./sqrt(db2pow(ACMTxGain));
% Cphase1 = Cphase1.*GainDif.';
for i=1:(NumCarrier+1)/2
    if i==1
        UpLinkH(i) = UpLinkH(i)*GainDif(i);
        for j=1:((NumCarrier+1)/2-1)
            UpLinkH(2*j) = UpLinkH(2*j)*GainDif(i);
        end
    else
        UpLinkH(2*i-1) = UpLinkH(2*i-1)*GainDif(i);
    end
end
%% 校准因子计算
UplinkC = UpLinkH.*(cos(P(1:NumCarrier)) + sin(P(1:NumCarrier))*1i);
Cphase2(1,:)=(UplinkC(1,:)./DownLinkH(1,:));
% Cphase2(1,:)=(UplinkC(1,:)./conj(DownlnkC(1,:)));
for j=3:2:NumCarrier
%     temp2((j-1)/2) = (Cphase2(1,2)/Cphase2(1,1));
%     temp1((j-1)/2) = Cphase2(1,j)/Cphase2(1,j-1);
%         temp=(Cphase2(1,j)/Cphase2(1,1))/(Cphase2(1,j-1)/(Cphase2(1,2)));
    temp=(Cphase2(1,j)/Cphase2(1,j-1))*(Cphase2(1,2)/Cphase2(1,1));
    CalibrateFactor(1,(j-1)/2)=temp;
end

end