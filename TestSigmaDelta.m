
% clear all;
% close all;

% CaseNum = 95;
% Iter = 1;
% FftSize = 8192;
% slideTime = 3;
% filename = ['D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case',num2str(CaseNum),'/UD_InData',num2str(Iter-1),'.txt']; 
% Data = dlmread(filename,'');
% RxSignalOneSlot1 = reshape(Data.',[16 FftSize*slideTime]);
% SigDownSampleRx = RxSignalOneSlot1(1:2:end,:) + RxSignalOneSlot1(2:2:end,:)*1i;

SigDownSampleRx = GW_Rx_MutiAntenna;

AntennaSelect = 5;
fs0 = 1e6;
Signal_0 = SigDownSampleRx;

%% cic up sample
fs1 = 1e6;
fs2 = 32e6;
R1 = fs2/fs1;
[TmpI_1] = cic3s_intp(real(Signal_0(AntennaSelect,:)),R1);
[TmpQ_1] = cic3s_intp(imag(Signal_0(AntennaSelect,:)),R1);
Signal_1 = (TmpI_1 + TmpQ_1*1i).';

%% Sigma-Delta
[TmpI_2] = SigmaDelta(TmpI_1);
[TmpQ_2] = SigmaDelta(TmpQ_1);
Signal_2 = TmpI_2 + TmpQ_2*1i;

%% cic down sample
fs3 = 32e6;
fs4 = 1e6;
R2 = fs3/fs4;
[TmpI_3] = cic3s_decm(TmpI_2,R2);
[TmpQ_3] = cic3s_decm(TmpQ_2,R2);
Signal_3 = (TmpI_3 + TmpQ_3*1i).';

%% halfband down sample
fs5 = 125000;
Signal_4 = TestDownsampling(Signal_3,fs4/fs5);

%% cic下采样到125K
R3 = fs3/fs5;
[TmpI_4] = cic3s_decm(TmpI_2,R3);
[TmpQ_4] = cic3s_decm(TmpQ_2,R3);
Signal_5 = (TmpI_4 + TmpQ_4*1i).';

[Pxif1, fif1] = pwelch(Signal_0(1,:),[], [], [],fs0);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('1Mhz原始信号');
[Pxif1, fif1] = pwelch(Signal_1(1,:),[], [], [],fs1);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('cic上采样32Mhz');
[Pxif1, fif1] = pwelch(Signal_2(1,:),[], [], [],fs2);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('32Mhz sigma-delta调制');
% [Pxif1, fif1] = pwelch(Signal_3(1,1:65536),[], [], [],fs3);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('cic下采样1Mhz');
[Pxif1, fif1] = pwelch(Signal_4(1,1:8192),[], [], [],fs5);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('halfband 1Mhz下采样125Khz');
[Pxif1, fif1] = pwelch(Signal_5(1,1:8192),[], [], [],fs5);figure;plot(fif1,10*log10(abs(Pxif1))); grid on;legend('cic32 Mhz下采样125Khz');


