function llr = Psk4Demodulation(symbol)
Psk4Set0 =  [1 2;1 4]; %分别对应第1~2个bit为0
Psk4Set1 =  [3 4;2 3]; %分别对应第1~2个bit为1
llr = zeros(1,2*length(symbol));
for i=1:length(symbol)
    Proj = zeros(1,4);
    % 计算symbol在8个方向上的投影
    Proj(1,1) = real(symbol(i));
    Proj(1,2) = imag(symbol(i));
    Proj(1,3) = -Proj(1,1);    
    Proj(1,4) = -Proj(1,2);
    
    llr2bits = zeros(1,2);
    for iC=1:2 % 计算3个bit的approximate llr
        tmp0 = max(Proj(Psk4Set0(iC,:)));
        tmp1 = max(Proj(Psk4Set1(iC,:)));
        llr2bits(1,iC) = tmp0-tmp1;
    end
    llr((i-1)*2+1:i*2) = llr2bits;
end
end