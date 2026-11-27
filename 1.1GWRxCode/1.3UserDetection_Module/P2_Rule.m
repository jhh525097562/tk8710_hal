function [P2,HighLightPos] = P2_Rule(APreambleFftESum1,HighLightPos,P,TempFFTLen,GWRXPara)

global MaxOutLen;
K = 10^(GWRXPara.GWRxP2_The/10);   %10000 40dB  3162 35dB

maxval = max(APreambleFftESum1);
the = maxval / K;

P2 = P;
for i=1:MaxOutLen
    if HighLightPos(i)==1
        P2(i) = 0;
    else
        cand = APreambleFftESum1(HighLightPos(i));
        if cand < the
            P2(i) = 0;
            HighLightPos(i) = 1;
        end
    end
end


end

