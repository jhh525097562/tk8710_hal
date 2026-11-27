function P3 = P3_Rule(APreambleFftESum1,APreambleFft1,HighLightPos,P,TempFFTLen,GWRXPara)
global MaxOutLen;
P3_The = 10^(GWRXPara.GWRxP3_The/10);


P3 = P;
for i=1:MaxOutLen
    if HighLightPos(i)==1
        P3(1,i) = 0;
    else
        a = HighLightPos(i);
        b = TempFFTLen + 4 - a;
        [x1 x2]=max(APreambleFftESum1(b-2:b+2));
        b = b + x2 - 3;
        %             r = find(HighLightPos == b);
        %             if isempty(r) == 0
        fa = APreambleFftESum1(a);
        fb = APreambleFftESum1(b);
        
        if fb > P3_The * fa%镜像必须低于20dB，该频点如果低于20dB时，则认为是镜像，减去与镜像相关性，如果没有低于20dB，则为用�?
            v1 = APreambleFft1(:,a);
            v2 = APreambleFft1(:,b);
            Len = length(v1);
            Size = size(v2,2);
            out = correlation(a,b,Len,Size,APreambleFft1,0,1);
            P3(1,i) = P3(i) * (1 - out);
        else
            
        end
        %             end
    end
    
end

end

