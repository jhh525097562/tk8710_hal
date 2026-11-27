function [periodEsti]=InterfrencePeriodicIdxCalcuNew(P1,TempFFTLen)
P2=P1;
MaxLen = TempFFTLen/32;
index = zeros(1,MaxLen);
for i=1:TempFFTLen-1
    difff(i)=P2(i+1)-P2(i);
end
signSig=sign(difff);
for i=1:length(signSig)-1
    if signSig(i)>0||signSig(i)<0
        trend(i)=signSig(i);
    elseif signSig(i)==0&&signSig(i+1)>=0
        trend(i)=1;
    elseif signSig(i)==0&&signSig(i+1)<0
        trend(i)=-1;
    end
end
for i=1:length(trend)-1
    diffTrend(i)=trend(i+1)-trend(i);
end
Tmpindex=find(diffTrend==-2);
LenTmpIndex = length(Tmpindex);
if LenTmpIndex>MaxLen
    LenTmpIndex = MaxLen;
    index = Tmpindex(1:MaxLen);
else
    index(1:LenTmpIndex) = Tmpindex;
end
if LenTmpIndex <= 1
    periodEsti = 0;
else
%     diffVal=diff(index);
    %HJH add New
    diffVal0=diff(index);
    diffVal = diffVal0(find(diffVal0>(TempFFTLen/128)));
    num=length(diffVal);
    if num<1
        periodEsti=0;
        return;
    end
    for i=1:num
        numElement(i)=GetShu(diffVal,num,diffVal(i));
    end
    [Val,Ind]=max(numElement);
    if Val >= 4%��4���ظ�����£���Ϊ��һ������
        periodEsti=diffVal(Ind(1));
    else
        periodEsti = 0;
    end
    
    if periodEsti > TempFFTLen/8%�������ڴ���1/8���򲻿���
        periodEsti = 0;
    end
    if periodEsti < TempFFTLen/128%��������С��1/8���򲻿���
        periodEsti = 0;
    end
end
%%-------------------------------------------------------------------------
    function arrNum=GetShu(arr,num,x)
        arrNum=0;
        for ii=1:num
%             if x==arr(ii)
%                 arrNum=arrNum+1;
%             end
%             if x==arr(ii) || x==arr(ii)-1 || x==arr(ii)+1
%                 arrNum=arrNum+1;
%             end
            %HJH add New
            if x>=arr(ii)-1 && x<=arr(ii)+1
                arrNum=arrNum+1;
            end
        end
    end
end