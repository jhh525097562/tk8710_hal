function   [Errfsk,Errdqpsk] =  ErrCountSim(InputBitsBlockSimSave,decoded_out_fsk,decoded_out_dqpsk,FskSegInfoBitLenSim,DqpskSegInfoBitLenSim,BlockHeaderLenSim,iBlock)

BlockTemp = [decoded_out_fsk decoded_out_dqpsk];
decoded_out_fsk =  BlockTemp(1: FskSegInfoBitLenSim + BlockHeaderLenSim/2);
decoded_out_dqpsk =  BlockTemp( FskSegInfoBitLenSim + BlockHeaderLenSim/2 + 1: FskSegInfoBitLenSim + DqpskSegInfoBitLenSim + BlockHeaderLenSim );
infoBits1 = InputBitsBlockSimSave(iBlock,1: FskSegInfoBitLenSim + BlockHeaderLenSim/2);
infoBits2 = InputBitsBlockSimSave(iBlock, FskSegInfoBitLenSim + BlockHeaderLenSim/2 + 1: FskSegInfoBitLenSim + DqpskSegInfoBitLenSim + BlockHeaderLenSim );

% fsk Error
if sum(abs(decoded_out_fsk - infoBits1)) ~= 0
    Errfsk = 1;
else
    Errfsk= 0;
end
% dqpsk Error
if sum(abs(decoded_out_dqpsk-infoBits2)) ~= 0
    Errdqpsk = 1;
else
    Errdqpsk= 0;
end

end