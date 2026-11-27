function [error2]=crc_decode(rev,ExternalCRCLen)

switch ExternalCRCLen
    case 7
     poly = [1 1 1 0 0 1 0 1];
    case 14
     poly = [1 0 0 0 1 1 0 1 1 1 0 0 0 1 1];
    case 15
     poly = [1 0 1 1 0 1 0 0 1 0 0 0 1 1 1 1];
end

N = ExternalCRCLen+1;

% [q r]=deconv(rev,poly);%得到长除法之后的商和余数，十进制下
% r=mod(abs(r),2);%转化为二进制下
% if r==zeros(1,length(rev))
%     error=0;
% else
%     error=-1;
% end

revTemp = rev(1:1+N-1 );
for ii = 1:length(rev)-N
    if revTemp(1) ==1
    M2 = xor( revTemp , poly);
    elseif revTemp(1) ==0
    M2 = xor( revTemp , zeros(1,N));
    end
    revTemp = [M2(2:end) rev( N + ii )];
end
if revTemp(1) == 1
    revTemp = xor( revTemp , poly);
end

if revTemp==zeros(1,N)
    error2=0;
else
    error2=1;
end


end