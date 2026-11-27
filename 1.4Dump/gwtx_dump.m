function gwtx_dump(GWTXAH,GWRxPilotPower,GWTXMode,GWTXFreq,GWTxData,Fbits1,Sbits1,GWRxUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor,Iter, case_num)
folder_name = ['/home/data/K2301/tx_bf/c_data/case' num2str(case_num) '/'];
mkdir(folder_name);
MACTxByte = GWTXPara.GWTxByteLen - 2;
PHYTxByte = GWTXPara.GWTxByteLen;
DataTxByte = GWTxData(1,:);
if GWTXPara.GWTxCRCEn == 1
    for ck0=1:GWTXPara.GWTxBlockNum
        for i=1:MACTxByte
            temp = bitget(DataTxByte(1,i+(ck0-1)*MACTxByte),8:-1:1,'uint8');
            infoBits(1,(8*i-7)+(ck0-1)*PHYTxByte*8:(8*i)+(ck0-1)*PHYTxByte*8) = temp;
        end
        infoBits(1,ck0*PHYTxByte*8-15) = Sbits1(ck0);
        infoBits(1,ck0*PHYTxByte*8-14) = Fbits1(ck0);
        infoBits(1,ck0*PHYTxByte*8-13:ck0*PHYTxByte*8) = zeros(1,14);        
    end
else
    for ck0 = 1 : GWTXPara.GWTxBlockNum
        for i = 1 : PHYTxByte
            temp = bitget(DataTxByte(1,i+(ck0-1)*PHYTxByte),8:-1:1,'uint8');
            infoBits(1,(8*i-7)+(ck0-1)*PHYTxByte*8:(8*i)+(ck0-1)*PHYTxByte*8) = temp;
        end
    end
end
filename = [folder_name 'in_1.txt'];
write2data(infoBits, filename, 'txt');

filename = [folder_name 'in_2.txt'];
GWTXAH_o = reshape(GWTXAH, 1, []);
write2data(GWTXAH_o, filename, 'txt');

filename = [folder_name 'in_3.txt'];
ANoise_o = reshape(ANoise, 1, []);
write2data(ANoise_o, filename, 'txt');

filename = [folder_name 'in_4.txt'];
write2data(CalibrateFactor, filename, 'txt');

filename = [folder_name 'in_5.txt'];
write2data(GWTXFreq, filename, 'txt');

filename = [folder_name 'in_6.txt'];
write2data(GWRxPilotPower, filename, 'txt');
end