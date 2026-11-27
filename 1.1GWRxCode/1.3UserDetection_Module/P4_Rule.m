
function P4 = P4_Rule(APreambleFftESum1,APreambleFftESum0,APreambleFft1,APreambleFft0,HighLightPos,P,TempFFTLen)

global MaxOutLen;


P4 = P;
for i=1:MaxOutLen
    if HighLightPos(i)==1
        P4(i) = 0;
    else
        a = HighLightPos(i);
        fa = APreambleFftESum1(a);
        if a==1
            x1=TempFFTLen;
            x2=a;
            x3 =a+1;
        elseif(a==TempFFTLen)
            x1=a-1;
            x2=a;
            x3 =1;
        else
            x1=a-1;
            x2=a;
            x3 =a+1;
        end
        fb = [APreambleFftESum0(x1) APreambleFftESum0(x2) APreambleFftESum0(x3)];
        y = f_func(fa,fb);
        v1 = APreambleFft1(:,a);
        v2 = APreambleFft0(:,a-1:a+1);
        Len = length(v1);
        Size = size(v2,2);
        out = correlation(a,[a-1:a+1],Len,Size,APreambleFft1,APreambleFft0,0);
        
        Q = sqrt(y).*out;
        P4(i) = P4(i) * (1 - max(Q));
    end
end
end
