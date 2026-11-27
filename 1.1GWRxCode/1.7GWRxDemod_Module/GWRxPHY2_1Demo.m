function [bitErrorInFrame,FSKSNR,DQPSKSNR,RxDataTemp,Sbit,Fbit] = GWRxPHY2_1Demo(SigDemodIn1,ModePara)

RxDataTemp(1:1000) = 0;
bitErrorInFrame(1:16) = 1;

[GWRXDemodParaOut] = PHY2_1RxDemod(SigDemodIn1,ModePara);

bitErrorInFrame = GWRXDemodParaOut.crc_err;
FSKSNR(1:length(GWRXDemodParaOut.demod_fsk_snr)) = GWRXDemodParaOut.demod_fsk_snr;
DQPSKSNR(1:length(GWRXDemodParaOut.demod_fsk_snr)) = GWRXDemodParaOut.demod_qpsk_snr;
RxDataTemp(1:length(GWRXDemodParaOut.RxData)) = GWRXDemodParaOut.RxData;
Sbit = GWRXDemodParaOut.Sbit;
Fbit = GWRXDemodParaOut.Fbit;
end