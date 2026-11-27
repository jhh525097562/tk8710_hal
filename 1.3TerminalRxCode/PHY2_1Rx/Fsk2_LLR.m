function   [demodBits1LLR, symRxCombine_best] = Fsk2_LLR(symRxCombine,SymNumDataCombine,avg_power,Mfsk)

symRxCombine_best = zeros(SymNumDataCombine,1);
indx_Sim = zeros(SymNumDataCombine,1);
indx_Sim(1) = Mfsk;
dem1=zeros(SymNumDataCombine,1);

for iSym=1:SymNumDataCombine
    rtmp = zeros(1,Mfsk);
    
    for ii = 1:Mfsk
        rtmp(ii) = abs( symRxCombine((iSym)*Mfsk+ii) )^2  ;
    end
    
    dem1(iSym) = rtmp(1) - rtmp(2);
    bit = fix(1 - (sign(dem1(iSym))) / 2);
    
    indx_Sim(iSym+1) = bit + 1;
    symRxCombine_best(iSym) = symRxCombine((iSym-1)*2+indx_Sim(iSym));
end
demodBits1LLR = reshape( [dem1].', 1 , [] );
demodBits1LLR = demodBits1LLR/avg_power;

% scatterplot( symRxCombine_best )
% scatterplot( demodBits1LLR )

end