function [AntennaANoise] = AnTennaGainAdjustModule_Test(APreambleFftETmp,Antenna,FFTLen,GWRXPara)
global ANoiseTemp;
global ANoiseDif;
global ANoiseThe;
AntennaANoise =zeros(Antenna,1);
for  Ai10= 1:Antenna
    Tmpm = 0;
    for i=1:FFTLen
        if(APreambleFftETmp(Ai10,i)<ANoiseThe(Ai10))
            Tmpm=Tmpm+1;
        end
    end
    
    if Tmpm<FFTLen*(6/32)
        AntennaANoise(Ai10,1)=ANoiseThe(Ai10)*2;
        Tmp(Ai10,1) = 2;
    elseif Tmpm>=FFTLen*(26/32)
        AntennaANoise(Ai10,1)=ANoiseThe(Ai10)*0.5;
        Tmp(Ai10,1) = 0.5;
    elseif Tmpm>=FFTLen*(6/32) && Tmpm<FFTLen*(15/32)
        AntennaANoise(Ai10,1)=ANoiseThe(Ai10)*1.0715;%10*log10(1.0715)
        Tmp(Ai10,1) = 1.0715;
    elseif Tmpm>=FFTLen*(17/32) && Tmpm<FFTLen*(26/32)
        AntennaANoise(Ai10,1)=ANoiseThe(Ai10)*0.9333;%10*log10(0.9333)
        Tmp(Ai10,1) = 0.9333;
    elseif Tmpm>=FFTLen*(15/32) && Tmpm<FFTLen*(17/16)
        AntennaANoise(Ai10,1)=ANoiseThe(Ai10);%10*log10(0.9333)
        Tmp(Ai10,1) = 1;        
    end
    
end
ANoiseDif = [ANoiseDif Tmp];
ANoiseTemp = [ANoiseTemp AntennaANoise];
end