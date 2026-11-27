
function P5 = P5_Rule(APreambleFftESum1,APreambleFft1,HighLightPos,P,TempFFTLen)
global MaxOutLen;

P5 = P;
for i=1:MaxOutLen
    if i==533
        k=0;
    end
    if HighLightPos(i)==1
        P5(i) = 0;
    else
        a = HighLightPos(i);
        fb = [APreambleFftESum1(a-1) APreambleFftESum1(a+1)];
        fa = APreambleFftESum1(a);
        x = h_func(fb,fa);
        v1 = APreambleFft1(:,a);
        v2 = APreambleFft1(:,a-1);
        v3 = APreambleFft1(:,a+1);
        Len = length(v1);
        Size1 = size(v2,2);
        Size2 = size(v3,2);
        out = [correlation(a,a-1,Len,Size1,APreambleFft1,0,1) correlation(a,a+1,Len,Size2,APreambleFft1,0,1)];
        Q = x.*out;
        P5(i) = P5(i) * (1 - max(Q));
    end
end
end
