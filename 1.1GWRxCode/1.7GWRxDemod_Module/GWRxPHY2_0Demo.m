function [bitErrorInFrame,FSKSNR,DQPSKSNR,RxDataTemp] = GWRxPHY2_0Demo(SigDemodIn1,SysSimulationPara,paraRxPHY,GWRxPara)

    RxDataTemp(1:1000) = 0;
    for Segi = 1:paraRxPHY.SegNum
        SigDemodIn2 = SigDemodIn1(1 - (Segi - 1)*paraRxPHY.C2Adj+(Segi - 1)*paraRxPHY.SegLen:Segi*paraRxPHY.SegLen ...
            - (Segi - 1)*paraRxPHY.C2Adj + paraRxPHY.TOScope);
        [bitErrorInFrame(1,Segi),FSKSNR(1,Segi),DQPSKSNR(1,Segi),RxDataTemp(1,1+(Segi-1)*paraRxPHY.NumByte:Segi*paraRxPHY.NumByte),...
            estTimingOffset,estFreqOffset] = PHY2_0RxDemod(SigDemodIn2,paraRxPHY,0,GWRxPara,SysSimulationPara);%
    end
    
end