function [GWRxUserInf,GWRxUserNumOut] = UserCheckModule(GWRxData,GWRxUserInf,GWRxUserNumIn,CRC,FSKSNR,DQPSKSNR,Freq,AH,RxPilotPower,AppointTChID,Sbit,Fbit,paraRxPHY,PdBTmp,ModePara)

GWRxUserNumOut = GWRxUserNumIn;
crc14_en = ModePara.crc14_en;
if paraRxPHY.Mfsk==4
%     TempGWRxUserInf.SNR = mean(FSKSNR(1:paraRxPHY.SegNum));
    TempGWRxUserInf.SNR = mean(FSKSNR(1:end));
else
%     TempGWRxUserInf.SNR = mean(DQPSKSNR(1:paraRxPHY.SegNum));
    TempGWRxUserInf.SNR = mean(DQPSKSNR(1:end));
end
% TempGWRxUserInf.CRC = sum(CRC(1:paraRxPHY.SegNum),2);
TempGWRxUserInf.CRC = sum(CRC(1:end),2);
TempGWRxUserInf.Freq = Freq;
%     TempGWRxUserInf(C2i).Freq = AFreqBinDMax(1,C2i);

% TempGWRxUserInf.RxData = GWRxData(1:paraRxPHY.SegNum*paraRxPHY.NumByte);
TempGWRxUserInf.RxData = GWRxData;
TempGWRxUserInf.RxMode = paraRxPHY.Mode;
TempGWRxUserInf.RxPilotPower = RxPilotPower;
TempGWRxUserInf.RxAH = AH(:,1);
TempGWRxUserInf.AppointTChID = AppointTChID;
TempGWRxUserInf.Sbit = Sbit;
TempGWRxUserInf.Fbit = Fbit;
TempGWRxUserInf.PdBTmp = PdBTmp;
% 锟斤拷取锟斤拷效锟矫伙拷锟斤拷息

GWRxUserInf(GWRxUserNumIn).SNR = 0;
GWRxUserInf(GWRxUserNumIn).CRC = 0;
GWRxUserInf(GWRxUserNumIn).Freq = 0;
GWRxUserInf(GWRxUserNumIn).RxData = 0;
GWRxUserInf(GWRxUserNumIn).RxMode = 0;
GWRxUserInf(GWRxUserNumIn).RxPilotPower = 0;
GWRxUserInf(GWRxUserNumIn).RxAH = 0;

if TempGWRxUserInf.CRC==0
    if crc14_en==1
        for it=1:GWRxUserNumIn
    %         if sum((abs(GWRxUserInf(it).RxData - TempGWRxUserInf.RxData)) ~= 0) ==0
            if isequal(GWRxUserInf(it).RxData,TempGWRxUserInf.RxData)
                if GWRxUserInf(it).RxPilotPower < TempGWRxUserInf.RxPilotPower
                    GWRxUserInf(it).SNR = TempGWRxUserInf.SNR;
                    GWRxUserInf(it).Freq = TempGWRxUserInf.Freq;
                    GWRxUserInf(it).RxPilotPower = TempGWRxUserInf.RxPilotPower;
                    GWRxUserInf(it).RxAH = TempGWRxUserInf.RxAH;
                end
                break;
            end
        end
    else
        it=GWRxUserNumIn;
    end
    if it==GWRxUserNumIn
        GWRxUserInf(GWRxUserNumIn).SNR = TempGWRxUserInf.SNR;
        GWRxUserInf(GWRxUserNumIn).CRC = TempGWRxUserInf.CRC;
        GWRxUserInf(GWRxUserNumIn).Freq = TempGWRxUserInf.Freq;
        GWRxUserInf(GWRxUserNumIn).RxData = TempGWRxUserInf.RxData;
        GWRxUserInf(GWRxUserNumIn).RxMode = TempGWRxUserInf.RxMode;
        GWRxUserInf(GWRxUserNumIn).RxPilotPower = TempGWRxUserInf.RxPilotPower;
        GWRxUserInf(GWRxUserNumIn).RxAH = TempGWRxUserInf.RxAH;
        GWRxUserInf(GWRxUserNumIn).AppointTChID = TempGWRxUserInf.AppointTChID;
        GWRxUserInf(GWRxUserNumIn).Sbit = TempGWRxUserInf.Sbit;
        GWRxUserInf(GWRxUserNumIn).Fbit = TempGWRxUserInf.Fbit;
        GWRxUserInf(GWRxUserNumIn).PdBTmp = TempGWRxUserInf.PdBTmp;
%         UserID(GWRxUserNumIn,1:2) = GWRxUserInf(GWRxUserNumIn).RxData(4:5);
        UserID(GWRxUserNumIn,1:2) = GWRxUserInf(GWRxUserNumIn).RxData(1:2);
        GWRxUserInf(GWRxUserNumIn).UserID = UserID(GWRxUserNumIn,1:2);
        GWRxUserNumOut = GWRxUserNumIn + 1;
        GWRxUserInf(it).TxMode = GWRxUserInf(GWRxUserNumIn).RxMode + 9;
    end
end
end