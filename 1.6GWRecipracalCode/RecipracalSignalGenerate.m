function [ACMTxSignal,DataTxSignal] = RecipracalSignalGenerate(GWRxAntennaNum,SelectACMAntenna,GWRxSampleRate,NumberFFT,Freq,P,ACMTxAmpc,DataTxAmpc)

% NumCarrier=GWRxAntennaNum*2-1;  %number of carrier 2*7-1
NumCarrier=15;  %number of carrier 2*7-1

EnableRF(1:GWRxAntennaNum) = ones(1,8);
EnableRF(SelectACMAntenna) = 0;
for i = 1:NumberFFT
    t(i) = 1/GWRxSampleRate * (i - 1); %观测点
end
ACMTxSignal = zeros(1,NumberFFT);
for i=1:NumCarrier
    Tone1(i,:) = cos(2*pi*Freq(i)*t-P(i)) + sin(2*pi*Freq(i)*t-P(i))*1i;
end

for i=1:GWRxAntennaNum
    if i==1
        ACMTxSignal = EnableRF(i)*ACMTxAmpc(i)*Tone1(1,:);
        for j=1:(GWRxAntennaNum-1)
            ACMTxSignal = ACMTxSignal + EnableRF(i)*ACMTxAmpc(i)*Tone1(j*2,:);
        end
    else
        ACMTxSignal = ACMTxSignal + EnableRF(i)*ACMTxAmpc(i)*Tone1(2*i-1,:);
    end
end

DataTxSignal = zeros(GWRxAntennaNum,NumberFFT);
for i=1:NumCarrier
    Tone2(i,:) = cos(2*pi*Freq(i)*t) + sin(2*pi*Freq(i)*t)*1i;
end
for i=1:GWRxAntennaNum
    if i==1
        DataTxSignal(i,:) = EnableRF(i)*DataTxAmpc(i)*Tone2(1,:);
        for j=1:(GWRxAntennaNum-1)
            DataTxSignal(i,:) = DataTxSignal(i,:) + EnableRF(i)*DataTxAmpc(i)*Tone2(j*2,:);
        end
    else
        DataTxSignal(i,:) = EnableRF(i)*DataTxAmpc(i)*Tone2(2*i-1,:);
    end
end


end