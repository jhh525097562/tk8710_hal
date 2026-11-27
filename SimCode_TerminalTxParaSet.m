function [TxFreq,TxMode,TxPower,TxData,Fbits,Sbits,TxImagePower,RxMode,PathAttenuation] = SimCode_TerminalTxParaSet(SimPara,TMTXPara,GWRXPara,GWTXPara,Iter)

s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);

TxUserNum = TMTXPara.TxUserNum;

TxFreq(1:TxUserNum+50) = 0;
TxMode(1:TxUserNum+50) = 0;
TxPower(1:TxUserNum+50) = 1;
TxData(1:TxUserNum,1:2000) = 0;
TxImagePower(1:TxUserNum+50) = 1;
PathAttenuation(1:TxUserNum+50) = 0;
RxMode(1:TxUserNum+50) = 0;
F1 = 0;
for j=1:TxUserNum
    if j <=TMTXPara.AllcationUserNum
        TxFreq(j) = TMTXPara.AllocationTXFreqSet(j);
        % TxMode
        TxMode(j) = TMTXPara.TMAllocationTxMode(j);
        % RxMode
        RxMode(j) = GWTXPara.GWTxMode;
        F1 = GWRXPara.GWRxSampleRate/2 - abs(TxFreq(j));
    else
        % TxMode
        TxMode(j) = TMTXPara.TMTxMode(1);
        % RxMode
        RxMode(j) = GWTXPara.GWTxMode(1);
        %TxFreq
        [BW] = GetModeBw(TxMode(j));
%         FreBand = BW/5;
        FreBand = BW/2;
        if SimPara.TXFreqType == 0%TM Rand Freq Tx
            FreStart = -GWRXPara.GWRxSampleRate/2 + F1;
            FreEnd =  GWRXPara.GWRxSampleRate/2;
            TxFreq(j) = randi([ceil(FreStart+FreBand),ceil(FreEnd-FreBand)],1);
        elseif SimPara.TXFreqType == 1%TM Fixed Freq Tx
            FreqStart = GWRXPara.GWRxSampleRate/5 + 500;
            TxFreq(j) = FreqStart + SimPara.TxFreqInterval(1,1)*j + (SimPara.TxFreqInterval(1,1)*6)*mod(Iter,2);
%             TxFreq(j) = FreqStart + SimPara.TxFreqInterval(1,1)*j + randi([800 2000],1)*mod(Iter,2);
        end

    end
    
    %TxDataPowr
    TxPower(j) = 50;
    if SimPara.TxPowerDiffSet==0
        TxPowerDiffSet = 0;
    else
        if SimPara.TxPowerDiffSet>0
            TxPowerDiffSet = randi(SimPara.TxPowerDiffSet);%
        else
            TxPowerDiffSet = -randi(abs(SimPara.TxPowerDiffSet));%
        end
    end
    if SimPara.Tx_AVGPowerDiffSet==0
        
    else
        if j<=SimPara.Tx_AVGPowerDiffUserNum
            TxPowerDiffSet = TxPowerDiffSet + SimPara.Tx_AVGPowerDiffSet;
        end
    end
    PathAttenuation(j) = TxPowerDiffSet;
    %TxData
    if j<256
        TxData(j,1) = 0;
        TxData(j,2) = j;
    else
        TxData(j,1) = 1;
        TxData(j,2) = j-255;
    end
    
    TxData(j,3:500) = floor(rand(1,498)*255);
    Fbits(j,:) = zeros(1,16);
    Fbits(j,1)= 1;
    Sbits(j,:) = ones(1,16);
    Sbits(j,end) = 0;
    %         TxData(j,1:end) = 1:1:length(TxData);
    %TxImagePower
    TxImagePower(j) = db2pow(SimPara.Tx_Image_InterferencePowerDiffSet);
end
end

function [BW] = GetModeBw(Mode)
if Mode<=3 || (Mode<=13 && Mode>=11)
    BW = 2000;
elseif Mode==10 || Mode==20
    BW = 125000;
else
    BW = 2000*2^(Mode-3);
end
end
