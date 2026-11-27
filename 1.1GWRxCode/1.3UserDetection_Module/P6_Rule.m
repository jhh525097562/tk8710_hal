
function  P6 = P6_Rule(APreambleFftESum1,APreambleFft1,HighLightPos,P,TempFFTLen,GWRXPara)
global MaxOutLen;

% if TempFFTLen<=8192
%     TempStart = 128*GWRXPara.GWRxShrink;
% else
%     TempStart = 128;
% end

% TempStart = 128*(TempFFTLen/8192);
TempStart = 20;

P6_The = 10^(GWRXPara.GWRxP6_The/10);

P6 = P;
for i=1:MaxOutLen
    if i==533
        k=0;
    end
    if HighLightPos(i)==1
        P6(i) = 0;
    else
        a = HighLightPos(i);
        amax = a + TempStart;
        amin = a - TempStart;
        Q0 = 0;
        QMax = 0;
        for j=1:TempStart
            if i-j>0
                if HighLightPos(i-j) >= amin
                    b = HighLightPos(i-j);
                    fb = APreambleFftESum1(b);
                    fa = APreambleFftESum1(a);
                    x = h_func(fb,P6_The*fa);
                    if x==1
                        v1 = APreambleFft1(:,a);
                        v2 = APreambleFft1(:,b);
                        %                 out = correlation(v1,v2,1);
                        Len = length(v1);
                        Size = size(v2,2);
                        %                 out = correlation(v1,v2,Len,Size);
                        Q0 = correlation(a,b,Len,Size,APreambleFft1,0,1);
                    else
                        Q0 = 0;
                    end
                    if Q0>QMax
                        QMax = Q0;
                    end
                end
            end
            if i+j<=MaxOutLen
                if HighLightPos(i+j) <= amax
                    b = HighLightPos(i+j);
                    fb = APreambleFftESum1(b);
                    fa = APreambleFftESum1(a);
                    x = h_func(fb,P6_The*fa);
                    if x==1
                        v1 = APreambleFft1(:,a);
                        v2 = APreambleFft1(:,b);
                        %                 out = correlation(v1,v2,1);
                        Len = length(v1);
                        Size = size(v2,2);
                        %                 out = correlation(v1,v2,Len,Size);
                        Q0 = correlation(a,b,Len,Size,APreambleFft1,0,1);
                    else
                        Q0 = 0;
                    end
                    if Q0>QMax
                        QMax = Q0;
                    end
                end
            end
        end
        P6(i) = P6(i) * (1 - QMax);
    end
end

end

