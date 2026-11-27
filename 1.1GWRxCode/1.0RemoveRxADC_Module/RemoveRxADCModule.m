function [SigRemoveADCOut,ADC] = RemoveRxADCModule(SigRxIn,Antenna,Len)

    ADC(1:Antenna,1) = mean(SigRxIn(1:Antenna,1:Len),2);
    SigRemoveADCOut = SigRxIn(1:Antenna,1:Len)-(diag((ADC(1:Antenna,1))))*ones(Antenna,Len);

end