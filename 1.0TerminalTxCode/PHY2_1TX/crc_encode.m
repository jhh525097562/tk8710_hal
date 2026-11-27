function [crc_encode_scr2 , crc2]=crc_encode(scr,ExternalCRCLen)

if ExternalCRCLen~=0

switch ExternalCRCLen
    case 7
     poly = [1 1 1 0 0 1 0 1];
    case 14
     poly = [1 0 0 0 1 1 0 1 1 1 0 0 0 1 1];
    case 15
     poly = [1 0 1 1 0 1 0 0 1 0 0 0 1 1 1 1];
end

N = ExternalCRCLen+1;
scrg=[scr.' zeros(1,N-1)];% 在信息多项式系数后补零

scrTemp = scrg(1:1+N-1 );
for ii = 1:length(scr)-1
    if scrTemp(1) ==1
    M2 = xor( scrTemp , poly);
    elseif scrTemp(1) ==0
    M2 = xor( scrTemp , zeros(1,N));
    end
    scrTemp = [M2(2:end) scrg( N + ii )];
end
if scrTemp(1) == 1
    scrTemp = xor(scrTemp , poly);
end
crc2 = scrTemp(2:end);
crc_encode_scr2 = [scr.' crc2];

% h2 = crc.generator('Polynomial',[1 0 1 1 0 1 0 0 1 0 0 0 1 1 1 1],'InitialState',[0 0 0 0 0 0 0 0 0 0 0 0 0 0 0],'FinalXOR',[0 0 0 0 0 0 0 0 0 0 0 0 0 0 0]);
% message_CRC      = generate(h2,scr);
% crc_encode_scr=bitor(scrg,r); % 余项加在信息位后面构成编码

elseif ExternalCRCLen==0
crc_encode_scr2 = scr.';
crc2 = [];
end

end