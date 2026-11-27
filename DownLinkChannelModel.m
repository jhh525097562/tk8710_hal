function [RxOut, TO] = DownLinkChannelModel(SimPara,GWAntennaTxPhaseNoReciprocal,GWTXPara,GW_Tx_MutiAntennaSignal,H_DownLink,N,ModeParaDown,PathAttenuation,Iter)
%CHANNELMODEL ¡¤¡é?¨ª¨ºy?Y?-1yD?¦Ì¨¤
%   Chan: D?¦Ì¨¤?¡êD¨ª
%   Tx: ¨º?¨¨?¨ºy?Y
% GWTX PhaseReciprocal
GW_Tx_MutiAntennaSignal = GW_Tx_MutiAntennaSignal.*GWAntennaTxPhaseNoReciprocal;
%
Txinput = H_DownLink*GW_Tx_MutiAntennaSignal;
s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);

SnrSamplingRate = SimPara.TMRxSNR-10*log10(GWTXPara.RFSampleRate/(ModeParaDown(1).BW*125/128))-pow2db(50); % ??CodedBit?¨´?¨º?????¨¤¦Ì¡À¨®¨²LenSym/4=4¡À?3?2¨¦?¨´, ¡ä??¨ª?aFs/4 = SymbolRate*4 ¡ê?¡Áa??¦Ì?samplingRate = 1M ¦Ì?SNRD¨¨¨°a10*log10(256)¦Ì?SNR213£¤
noiseVar = 1/10^(SnrSamplingRate/10);
parfor it=1:N
% for it=1:N
    [Rx(:,it),TO(it)] = OneUserChannelModel(Txinput(it,:),SimPara,GWTXPara.RFSampleRate,SnrSamplingRate,PathAttenuation(it));
    Noise = (randn(length(Rx(:,it)),1) + (1j)*randn(length(Rx(:,it)),1)) * sqrt(noiseVar/2);
    Rx(:,it) = Rx(:,it) + Noise;
end

Rxtemp = Rx.';
RxOut = Rxtemp;

end

