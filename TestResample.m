
clear all;
close all;
%% 
% SignalLen = 1000000;
% tmpO = 0:1:SignalLen-1;
% TestRxTemp = zeros(1,SignalLen);
% Freq = 20000;
% SampleRate = 1000000;
% TestRxTemp = exp(1i*2*pi*tmpO*Freq/SampleRate);
% NewSampleRate = SampleRate + 40;
% 
% TestRasampleData = resample(TestRxTemp,NewSampleRate,SampleRate);
% figure;plot(real(TestRxTemp(1:end)));hold on;
% plot(real(TestRasampleData(1:end)));legend('OldSampleRate','NewSampleRate');


SampleRate = 32e6;
time = 0.55;%s
SignalLen = time*SampleRate;
tmpO = 0:1:SignalLen-1;
TestRxTemp = zeros(1,SignalLen);
Freq = 20000;

TestRxTemp = exp(1i*2*pi*tmpO*Freq/SampleRate);

% load('sigRx.mat');
% TestRxTemp = sigRx;

Tdif = SampleRate/1e6 * 40;
NewSampleRate = SampleRate + Tdif;

TestRasampleData = resample(TestRxTemp,NewSampleRate,SampleRate);

% for k=1:1
%     T = k*5;
%     ViewLen = 0.001*SampleRate;
%     figure;plot(real(TestRxTemp(ViewLen*(T-1)+1:ViewLen*T)));hold on;
%     plot(real(TestRasampleData(ViewLen*(T-1)+1:ViewLen*T)));legend('OldSampleRate','NewSampleRate');
% end

% Hd_halfband = tx_halfband_design;
% HBFilter = Hd_halfband.Numerator;
% 
% Data0 = Downsample(TestRxTemp,SampleRate,500000,HBFilter);
% Data1 = Downsample(TestRasampleData,NewSampleRate,500000,HBFilter);
% for k=1:5
%     T = k*5;
%     ViewLen = 0.01*500000;
%     figure;plot(real(Data0(ViewLen*(T-1)+1:ViewLen*T)));hold on;
%     plot(real(Data1(ViewLen*(T-1)+1:ViewLen*T)));legend('OldSampleRate','NewSampleRate');
% end

in_rate = 1;
out_rate = 1+40/1e6;
RateConvert_buffer.window = zeros(1,4);
RateConvert_buffer.counter = 0;
[TestRasampleData1, RateConvert_buffer, result_tmpMiu] = dvbRateConvert(TestRxTemp, in_rate, out_rate, 1, 0.25, RateConvert_buffer);

% figure;plot(real(TestRasampleData(1:end)));hold on;
% plot(real(TestRasampleData1(1:end)));legend('OldSampleRate','NewSampleRate');
figure;plot(real(TestRasampleData(1:SignalLen)-TestRasampleData1(2:SignalLen+1)));
% figure;plot(real(TestRasampleData(1:SignalLen)));hold on;plot(real(TestRxTemp(1:end)));