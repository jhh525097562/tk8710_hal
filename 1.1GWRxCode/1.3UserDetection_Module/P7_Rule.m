
function P7 = P7_Rule(APreambleFftESum1,APreambleFft1,HighLightPos,P,Idx,TempFFTLen)
global MaxOutLen;

P7 = P;
if Idx ==0
    P7 = P;
    return;
end
for i=1:MaxOutLen
    if HighLightPos(i)==1
        P7(i) = 0;
    else
        a = HighLightPos(i);
        fa = APreambleFftESum1(a);
        b(1) = a - Idx;
        b(2) = a + Idx;
        for j = 1:length(b)
            if b(j) > TempFFTLen
                b(j) = b(j) - TempFFTLen;
            end
            if b(j) < 0
                b(j) = b(j) + TempFFTLen;
            end
            %             [x1 x2]=max(APreambleFftESum1(b(j)-2:b(j)+2));
            %             b(j) = b(j) + x2 - 3;
            c(1) = b(j)-1;
            c(2) = b(j)+1;
            if b(j)-1<1
                c(1) = 1;
            end
            if b(j)+1>TempFFTLen
                c(2) = TempFFTLen;
            end
            [x1 x2]=max(APreambleFftESum1(c(1):c(2)));
            b(j) = b(j) + x2 - 2;
            if b(j) >= TempFFTLen
                b(j) = TempFFTLen;
            end
            if b(j) <= 0
                b(j) = 1;
            end
        end
        y = 1;
        v1 = APreambleFft1(:,a);
        v2 = APreambleFft1(:,b);
        Len = length(v1);
        Size = size(v2,2);
        out = correlation(a,b,Len,Size,APreambleFft1,0,1);
        Q = y.*out;
        P7(i) = P7(i) * (1 - max(Q));
    end
    
end

end
