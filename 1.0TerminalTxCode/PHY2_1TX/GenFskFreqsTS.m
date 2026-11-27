function [FskFreqs,FskFreqsLoc,FskPhase] = GenFskFreqsTS(Mfsk,LenSym,LenSymPhase)

switch Mfsk
case 2
    
LenSymTmp = LenSym*8;
iTmp = 0:1:LenSymTmp-1;
tmpP=  0.5*iTmp/LenSymTmp ;
freq2 = exp(1i*2*pi*tmpP).';
tmpP=  -0.5*iTmp/LenSymTmp ;
freq3 = exp(1i*2*pi*tmpP).';

LenSymTmp = LenSymPhase;
iTmpPhase = 0:1:LenSymPhase-1;
tmpP1=  (0.5*iTmpPhase/LenSymTmp)*2*pi;
phase2 = tmpP1.';
tmpP1=  (-0.5*iTmpPhase/LenSymTmp)*2*pi;
phase3 = tmpP1.';

N1= 5;
FskFreqs = [ freq2(N1:8:end)  freq3(N1:8:end) ];
FskPhase = [ phase2  phase3 ];

FskFreqsLoc = FskFreqs';
% tmpCorr = FskFreqsLoc*FskFreqs; % 检查正交性
 
case 4

LenSymTmp = LenSym*4;
iTmp = 0:1:LenSymTmp-1;
tmpP=  1.5*iTmp/LenSymTmp ;
freq1 = exp(1i*2*pi*tmpP).';
tmpP=  0.5*iTmp/LenSymTmp ;
freq2 = exp(1i*2*pi*tmpP).';
tmpP=  -0.5*iTmp/LenSymTmp ;
freq3 = exp(1i*2*pi*tmpP).';
tmpP=  -1.5*iTmp/LenSymTmp ;
freq4 = exp(1i*2*pi*tmpP).';

LenSymTmp = LenSymPhase;
iTmpPhase = 0:1:LenSymPhase-1;
tmpP1=  (1.5*iTmpPhase/LenSymTmp)*2*pi;
phase1 = tmpP1.';
tmpP1=  (0.5*iTmpPhase/LenSymTmp)*2*pi;
phase2 = tmpP1.';
tmpP1=  (-0.5*iTmpPhase/LenSymTmp)*2*pi;
phase3 = tmpP1.';
tmpP1=  (-1.5*iTmpPhase/LenSymTmp)*2*pi;
phase4 = tmpP1.';

N1= 3;
FskFreqs = [freq1(N1:4:end) freq2(N1:4:end) freq3(N1:4:end) freq4(N1:4:end)];
FskPhase = [phase1 phase2 phase3 phase4];

FskFreqsLoc = FskFreqs';
% tmpCorr = FskFreqsLoc*FskFreqs; % 检查正交性


end

end