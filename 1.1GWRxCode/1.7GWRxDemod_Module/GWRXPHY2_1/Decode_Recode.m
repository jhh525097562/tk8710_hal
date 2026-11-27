function  [DecodeOut,RecodeOut] = Decode_Recode(FskDecodeOut,DeInterlvLLR,SecFlag)

        % quant
        bitLen = 6;
        bitLimit = 2^(bitLen-1)-1;
        demodBits1_quant = - round(DeInterlvLLR*bitLimit);
        demodBits1_quant(find(demodBits1_quant>bitLimit)) = bitLimit;
        demodBits1_quant(find(demodBits1_quant<-bitLimit)) = -bitLimit;
        
        % decode
        [DecodeOut] = CCdecoding(demodBits1_quant, bitLen);

        % recode
        if SecFlag == 0
            RecodeOut = CCEncoder(DecodeOut);
        elseif SecFlag == 1
            RecodeOut = [];
        end


end

















