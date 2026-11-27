function [AntennaANoise] = AnTennaGainAdjustModule(APreambleFftETmp,Antenna,FFTLen,GWRXPara)

    AntennaANoise =zeros(Antenna,1);
    TestCount(1:Antenna) = 0;
    for  Ai10= 1:Antenna
        AMax=max(APreambleFftETmp(Ai10,:));
        AMin=min(APreambleFftETmp(Ai10,:));

        if(AMax==AMin);
            AntennaANoise(Ai10,1)=1;
        else
%             while(1)
            for k=1:7%10
                Tmpm=0;
                TestCount(Ai10) = TestCount(Ai10) + 1;
                threshold=sqrt(AMin*AMax);
                for i=1:FFTLen
                    if(APreambleFftETmp(Ai10,i)<threshold);
                        Tmpm=Tmpm+1;
                    end
                end

                if(Tmpm<FFTLen/GWRXPara.GWRX_ANoiseThe1);
                    AMin=threshold;
                else(Tmpm>=FFTLen/GWRXPara.GWRX_ANoiseThe1);
                    AMax=threshold;
                end
                Tmpn=AMax/AMin;
                AntennaANoise(Ai10,1)=sqrt(AMin*AMax);
                if(Tmpn<=GWRXPara.GWRX_ANoiseThe2/GWRXPara.GWRX_ANoiseThe1);
                    break;
                end

                if(TestCount(Ai10)>=7)
                    TestCount(Ai10)
                    break;
                end
            end
        end
    end
end