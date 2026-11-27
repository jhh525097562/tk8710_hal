function [SignalFloat,SignalInt_M,SignalInt_C] = fixModule(InPutSignal,wSigNoiseRx,qSigNoiseRx,w2SigNoiseRx,SNRIn)

% wSigNoiseRx: 字宽
% qSigNoiseRx: 全小数
% w2SigNoiseRx: 信号动态范围，高动态：12，低动态：8，仿真经验最大值不满幅的情况
row = length(InPutSignal(:,1));
col = length(InPutSignal(1,:));

SNRGain = 29;
BitGain = 12;
MaxBitLen = 14;
SnrTmp = floor(SNRIn);
K = (SnrTmp - SNRGain);
Tmp = BitGain + floor(K/6);
if Tmp<=6
   Tmp = 6;
end
% if Tmp>=14
%    Tmp = 14; 
% end

% Tmp = w2SigNoiseRx;
for i=1:row
    SigNoiseRx = InPutSignal(i,:);
    SigNoiseRxMax = max(abs([real(SigNoiseRx) imag(SigNoiseRx)])); % 经验最大值
    SigNoiseRx = SigNoiseRx/SigNoiseRxMax; % 归一化
    SigNoiseRx = SigNoiseRx*(2^(Tmp-1)-1); % 将用经验最大值归一化放大到w2SigNoiseRx指定的最大幅度
    if Tmp>14
        index1 = real(SigNoiseRx)>2^(MaxBitLen-1)-1;index2 = imag(SigNoiseRx)>2^(MaxBitLen-1)-1;
        SigNoiseRx(index1) = 2^(MaxBitLen-1)-1 + imag(SigNoiseRx(index1))*1i;SigNoiseRx(index2) = real(SigNoiseRx(index2)) + (2^(MaxBitLen-1)-1)*1i;
        index3 = real(SigNoiseRx)<-2^(MaxBitLen-1)+1;index4 = imag(SigNoiseRx)<-2^(MaxBitLen-1)+1;
        SigNoiseRx(index3) = -2^(MaxBitLen-1)+1 + imag(SigNoiseRx(index3))*1i;SigNoiseRx(index4) = real(SigNoiseRx(index4)) + (-2^(MaxBitLen-1)+1)*1i;
    end
    SignalFloat(i,:) = SigNoiseRx/2^(wSigNoiseRx-1); % 生成float，供浮点代码的输入用
    SignalInt_M(i,:) = round(SigNoiseRx)/2^(wSigNoiseRx-1); % 生成fixed，供matlab定点代码的输入用
    SignalInt_C(i,:) = SignalInt_M(i,:)*2^qSigNoiseRx; % 生成整数，供c语言定点代码的输入用
end

end
