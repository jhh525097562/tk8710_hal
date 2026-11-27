clear all;
close all;

load('ACMTxGain.mat');
load('DataTxGain.mat');

global P;
P =[-1.02324779750120,-2.58039305874270,-2.78523111484624,0.363193019902249,1.29378952938372,2.13681484724016,-1.00220928762122,0.207595752893231,-3.02187936892107,0.936349644728564,-2.36334565934660,0.126400964528446,-0.0828443127029830,1.02527778844203,0.371577395760742];
global NumCarrier;
NumCarrier = 15;

%% 设置通道8为ACM时
load('UpLinkH.mat');
load('DownLinkH.mat');

[CalibrateFactor] = GWRecipracalALG(UpLinkH,DownLinkH,DataTxGain,ACMTxGain);

%% 设置通道7为ACM时
load('UpLinkH1.mat');
load('DownLinkH1.mat');

[CalibrateFactor1] = GWRecipracalALG(UpLinkH1,DownLinkH1,DataTxGain,ACMTxGain);

%% 两次校准的结果一起得到完整8根天线的校准因子
CalibrateFactorOut(1) = 1;
CalibrateFactorOut(2:7) = CalibrateFactor(1:6);
CalibrateFactorOut(8) = CalibrateFactor1(7);