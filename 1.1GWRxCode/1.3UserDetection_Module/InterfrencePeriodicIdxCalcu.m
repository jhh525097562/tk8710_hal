function [PeriodicIdx] = InterfrencePeriodicIdxCalcu(P1,TempFFTLen)

tmp0 = abs(ifft(P1)); % to time domain
tmp0(1:4) = 0;
% figure;plot(tmp0);
tmp1 = zeros(1,length(tmp0));
maxT = max(tmp0);
for i=1:length(tmp0) % 提取有意义的脉冲
    if tmp0(i)<maxT*0.25
        tmp1(i) = 0;
    elseif tmp0(i)>maxT*0.75
        tmp1(i) = maxT*0.5;
    else
        tmp1(i) = tmp0(i) - maxT*0.25;
    end
end
tmp1fft = fft(tmp1);
tmp0 = abs(tmp1fft); % to freq domain
% figure;plot(tmp0);
% tmp0(1:10) = 0;
tmp0(1:32) = 0;
maxT = max(tmp0(1:length(tmp0)/2));
% tmp0(1:4) = 0;
% maxT = max(tmp0(1:length(tmp0)));
for i=1:length(tmp0) % 提取第一个显著脉�?
    if tmp0(i)>maxT*0.75
        break;
    end
end

if i == TempFFTLen
    [tmpM,tmpI] = max([tmp0(TempFFTLen) tmp0(1) tmp0(2)]);
else if i == TempFFTLen-1
        [tmpM,tmpI] = max([tmp0(i) tmp0(i+1) tmp0(1)]);
    else
        [tmpM,tmpI] = max([tmp0(i) tmp0(i+1) tmp0(i+2)]);
    end
end
PeriodicIdx(1) = (i-1) + (tmpI-1);

if PeriodicIdx > TempFFTLen/8
    PeriodicIdx = 0;
end

%% 20220919 使用相关性确定周期干扰，假设干扰周期为T时，则有P1(x)与P1(x+T)的相关�?�高
% PeriodicIdx = 0;
% TempP1(1:TempFFTLen/2) = P1(1:TempFFTLen/2);
% for i=10:TempFFTLen/32
%     v1 = TempP1;
%     v2 = [TempP1(i+1:end) TempP1(end-i+1:end)];
%     tmp1 = sum(v1.*v2);
%     temP1 = sqrt(sum(v1));
%     temp3 = sqrt(sum(v2));
%     Coff1(i) = tmp1/temP1/temp3;
%     if Coff1(i) > 0.4
%         v1 = TempP1;
%         v2 = [TempP1(i*2+1:end) TempP1(end-i*2+1:end)];
%         tmp1 = sum(v1.*v2);
%         temP1 = sqrt(sum(v1));
%         temp3 = sqrt(sum(v2));
%         Coff1(2*i) = tmp1/temP1/temp3;
%         if Coff1(2*i) > 0.5
%             PeriodicIdx = i;
%             break;
%         end
%     end
% end
% 
% if PeriodicIdx > TempFFTLen/32
%     PeriodicIdx = 0;
% end

end