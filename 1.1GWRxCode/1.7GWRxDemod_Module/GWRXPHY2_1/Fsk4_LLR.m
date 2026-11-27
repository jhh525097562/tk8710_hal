function   [demodBits1LLR,avg_qpsk] = Fsk4_LLR(symRxCombine,SymNumDataCombine,avg_power,Mfsk)

symRxCombine_best = zeros(SymNumDataCombine,1);
indx_Sim = zeros(SymNumDataCombine,1);
indx_Sim(1) = 1;
dem1=zeros(SymNumDataCombine,1);
dem2=zeros(SymNumDataCombine,1);

% if EnNCoFsk == 0
    
for iSym=1:SymNumDataCombine
    rtmp = zeros(1,Mfsk);
    for ii = 1:Mfsk
        rtmp(ii) = abs( symRxCombine((iSym)*Mfsk+ii) )^2  ;
    end
    % graymapping 00 1  01 2  11 3   10 4
    tmpmax0 = max(rtmp(1),rtmp(2));
    tmpmax1 = max(rtmp(3),rtmp(4));
    dem1(iSym) = tmpmax0-tmpmax1;
    
    tmpmax0 = max(rtmp(1),rtmp(4));
    tmpmax1 = max(rtmp(2),rtmp(3));
    dem2(iSym) = tmpmax0-tmpmax1;
    
    bit0 = fix(1 - (sign(dem1(iSym))) / 2);
    bit1 = fix(1 - (sign(dem2(iSym))) / 2);
    
    indx_Sim(iSym+1) = Graymapping([],[],bit0,bit1,Mfsk);
    symRxCombine_best(iSym) = symRxCombine((iSym-1)*4+indx_Sim(iSym));
end
demodBits1LLR = reshape( [dem1 dem2].', 1 , [] );
demodBits1LLR = demodBits1LLR/avg_power;
%
avg_qpsk = mean(symRxCombine_best .* conj(symRxCombine_best));

% elseif EnNCoFsk == 1
%     
%  for iSym=1:SymNumDataCombine
%     rtmp = zeros(1,Mfsk);
%     for ii = 1:Mfsk
%         rtmp(ii) = symRxCombineNC((iSym)*Mfsk+ii)  ;
%     end
%     % graymapping 00 1  01 2  11 3   10 4
%     tmpmax0 = max(rtmp(1),rtmp(2));
%     tmpmax1 = max(rtmp(3),rtmp(4));
%     dem1(iSym) = tmpmax0-tmpmax1;
%     
%     tmpmax0 = max(rtmp(1),rtmp(4));
%     tmpmax1 = max(rtmp(2),rtmp(3));
%     dem2(iSym) = tmpmax0-tmpmax1;
%     
%     bit0 = fix(1 - (sign(dem1(iSym))) / 2);
%     bit1 = fix(1 - (sign(dem2(iSym))) / 2);
%     
%     indx_Sim(iSym+1) = Graymapping([],[],bit0,bit1,Mfsk);
%     symRxCombine_best(iSym) = symRxCombineNC((iSym-1)*4+indx_Sim(iSym));
% end
% demodBits1LLR = reshape( [dem1 dem2].', 1 , [] );
% demodBits1LLR = demodBits1LLR/avg_power;   
% %
% avg_qpsk = mean(symRxCombine_best);    
% 
% end


% scatterplot( symRxCombine_best )
% scatterplot( demodBits1LLR )
end