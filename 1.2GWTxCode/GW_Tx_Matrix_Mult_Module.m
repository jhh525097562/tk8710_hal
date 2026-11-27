function  [GW_Tx_MutiAntennaSignal] =  GW_Tx_Matrix_Mult_Module(TxSig,BeamformerMatrix,Matrix_MultLen,Matrix_MultCount)

for i=1:Matrix_MultCount
    
    GW_Tx_MutiAntennaSignal(:,1+(i-1)*Matrix_MultLen:i*Matrix_MultLen) = BeamformerMatrix.' * TxSig(:,1+(i-1)*Matrix_MultLen:i*Matrix_MultLen);
    
end

end