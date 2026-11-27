function [TerminalTxSignal,BW] = PHY2_1TerminalTxMainCode(SimPara,TMTXPara,GWRXPara,TxFreq,TxMode,TxPower,TxData,Fbits,Sbits,TxImagePower)

%     TerminalTxSignal(1:TxUserNum,1:200000) = 0;
%     TxUserNum = TMTXPara.TxUserNum;
TxUserNum = 1;
BW(1:TxUserNum) = 0;
for tempi=1:TxUserNum
    model = TxMode(tempi);
    %% 初始化参数计算
    %         ModeParaUp = [];
    [ModeParaUp] = getmodelparaTMTxPHY2_1(model,TMTXPara,GWRXPara);
    BW(tempi) = ModeParaUp.BW;
    %% 发射机
    MACTxByte = ModeParaUp.Len_TxByte - 2;
    PHYTxByte = ModeParaUp.Len_TxByte;
    DataTxByte = TxData(tempi,:);
    infoBits = [];
    if ModeParaUp.crc14_en == 1
        for j=1:ModeParaUp.TxBlockNum
            for i=1:MACTxByte
                temp = bitget(DataTxByte(tempi,i+(j-1)*MACTxByte),8:-1:1,'uint8');
                infoBits(1,(8*i-7)+(j-1)*PHYTxByte*8:(8*i)+(j-1)*PHYTxByte*8) = temp;
            end
            infoBits(1,j*PHYTxByte*8-15) = Sbits(j);
            infoBits(1,j*PHYTxByte*8-14) = Fbits(j);
            infoBits(1,j*PHYTxByte*8-13:j*PHYTxByte*8) = zeros(1,14);
        end
    else
        for j=1:ModeParaUp.TxBlockNum
            for i=1:PHYTxByte
                temp = bitget(DataTxByte(tempi,i+(j-1)*PHYTxByte),8:-1:1,'uint8');
                infoBits(1,(8*i-7)+(j-1)*PHYTxByte*8:(8*i)+(j-1)*PHYTxByte*8) = temp;
            end
        end
    end
    infoBits = infoBits.';
    InfoBitsOneSlot = infoBits;
    %% 模块1： DPFSK信号调制模块   输入：输入比特（整个slot），包块比特长度， 编码器选择， 码长， 包数， fsk模式， 扩频模式    输出：编码比特（整个slot）  for循环包块
    [SigTxSym] = PHY2_1Tx(InfoBitsOneSlot,ModeParaUp);
    m = log2(TMTXPara.SampleRate/ModeParaUp.FsTx);
    UpSampleSigTxSym0 = upsampling_new(SigTxSym,m);
    
    DataPreLen = (ModeParaUp.PilotLen + ModeParaUp.AGCLen)*TMTXPara.SampleRate;
    UpSampleSigTxSym = [-1*ones(1,DataPreLen) UpSampleSigTxSym0];
    sigTxUpsampleOut = exp(1*pi*1j*UpSampleSigTxSym);
    sigTxUpsampleOut = sigTxUpsampleOut(1:end-2^m+1);
    tmpO = 0:1:length(sigTxUpsampleOut)-1;
    SigTxOut = sigTxUpsampleOut.*exp(1i*2*pi*tmpO*TxFreq(tempi)/TMTXPara.SampleRate);
    %         figure;plot(phase(sigTxUpsampleOut));
    %% 增加发送镜像
    if SimPara.Tx_Image_InterferenceFlag ==0
        TxImageData = SigTxOut;
    else
        SigTxOut1 = sigTxUpsampleOut.*exp(1i*2*pi*tmpO*(-TxFreq(tempi))/SysSimulationPara.TerminalTxSampleRate);
        TxImageData = SigTxOut + SigTxOut1.*sqrt(TxImagePower(tempi));
    end
    %% 设置发送功率
    %         TerminalTxSignal(tempi,1:length(TxImageData)) = TxImageData.*sqrt(TxPower(tempi));
    TerminalTxSignal(tempi,:) = TxImageData.*sqrt(TxPower(tempi));
    %         [Pxif1, fif1] = pwelch(TerminalTxSignal(tempi,1:length(TxImageData)),[], [], [],SysSimulationPara.TerminalTxSampleRate);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;
end
end
