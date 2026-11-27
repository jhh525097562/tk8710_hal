
function [SigRxOut,index,PdBTmp] = BeamFormingSelectionModule(SigRxIn,AH,Antenna,slideNum,DSSS,ifftLen,FftSize,BeamformerMatrix,GWRXBFMode)
index=1;
PdBTmp = 0;
if GWRXBFMode==0
    GWRXBFSelectAlpha_REG = 0.9;%0-1,step:0.01
    GWRX_BFSelectThe_REG = 4;%0-8,step:1
    slideDataNum_REG = 4;%0-16,step:1
    
    load('BPSKTable20221019.mat');
    load('PSKTable20230213.mat');
    SNRTable = PSKTable;
    SNRTable(:,3) = PSKTable(:,3);
    SNRTable(:,4) = (10.^(PSKTable(:,2)./10))./(10.^(PSKTable(:,2)./10)+1);
    
    
    % ?????????
    AHNorm = AH;
    %     powExp = sum(abs(AHNorm).^2)/sqrt(FftSize)*(8192/FftSize)*sqrt(DSSS);
    %     powExp = sum(abs(AHNorm).^2)*(8192/FftSize)*sqrt(DSSS);
    powExp = sum(abs(AHNorm).^2)*sqrt(DSSS);
    alpha = GWRXBFSelectAlpha_REG;
    
    %% ?$?????????????????????
    % ??????E??
    slideSectionlen = ifftLen/slideDataNum_REG;
    SumdataAbs = zeros(1,Antenna);
    for ij = 1:slideDataNum_REG
        if ij == 1
            slideSectionlenstart = ceil(slideSectionlen/slideDataNum_REG);
        elseif ij ==slideDataNum_REG
            slideSectionlenstart = ceil(-(slideSectionlen/slideDataNum_REG));
        else
            slideSectionlenstart = 0;
        end
        %new
        dataAbs = abs(SigRxIn(:,(ij-1)*slideSectionlen+1+slideSectionlenstart:ij*slideSectionlen+slideSectionlenstart)).^2;
        TmpSumdataAbs = sum(dataAbs,2).';
        TmpP = 1./(TmpSumdataAbs./length(dataAbs));
        for k=1:Antenna
            SumdataAbs(k) = SumdataAbs(k) + TmpSumdataAbs(k);
        end
        
        for k=1:Antenna
            dataNormal(k,(ij-1)*slideSectionlen+1:ij*slideSectionlen) = (dataAbs(k,:).*TmpP(k)).^2;
        end
        
    end
    P = SumdataAbs.'/ifftLen;
    E = mean(dataNormal,2);
    if 1
%         P = SumdataAbs.';
%         powExp = 1;
    powExp = powExp/(ifftLen^2);
    end
    %% ?????????
    select = zeros(Antenna,1);
    SNRSelect1 = zeros(Antenna,1);
    PdB = zeros(Antenna,1);
    
    if slideNum <= 1
        Table = BPSKTable;
    else
        Table = SNRTable;
    end
    
    for ll = 1:Antenna
        diff = abs(E(ll)-Table(:,3));
        [tempmin,index] = min(diff);
        SNRSelect1(ll) = Table(index,2)-ll*0.2;
        %         Pmsk(ll,1) = P(ll)*10^(SNRSelect1(ll)/10)/(10^(SNRSelect1(ll)/10)+1);
        Pmsk(ll,1) = P(ll)*Table(index,4);
        %         PdB(ll) = abs(10*log10(Pmsk(ll,1)/powExp)+3)^GWRX_BFSelectThe;
        PdBTmp = 10*log10(Pmsk(ll,1)/powExp) - 1.5;
        %         PdBTmp = 10*log10(Pmsk(ll,1)/powExp) + 1.5;
        if PdBTmp>=0
            PdB(ll) = PdBTmp^GWRX_BFSelectThe_REG;
        else
            PdB(ll) = PdBTmp^(GWRX_BFSelectThe_REG/2);
        end
        select(ll) = alpha*(SNRSelect1(ll)) - (1-alpha)*PdB(ll);
    end
    [~,index] = max(select);
    SoftOut = 1;
    %%
    SigRxOut = SigRxIn(index,:) * SoftOut;
elseif GWRXBFMode ==1
    SigRxOut = BeamformerMatrix(1,:)*SigRxIn;
elseif GWRXBFMode ==2
    SigRxOut = SigRxIn(1,:);
end

end