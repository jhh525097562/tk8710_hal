clc;
clear all;
close all;

addpath(genpath('1.3TerminalRxCode'));
addpath(genpath('1.2GWTxCode'));
addpath(genpath('1.1GWRxCode'));
addpath(genpath('1.0TerminalTxCode'));
addpath(genpath('Log'));

TestCasePara = xlsread('TestCaseListNew3.xlsx');
TestCaseList = [120:135];%138
SimNum = 1000;
parfor it = 1:length(TestCaseList)
% for it = 1:length(TestCaseList)
    k = TestCaseList(it);
    %SysParaIn
    [SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara] = TestCaseListParaSet(TestCasePara(k,:));

    [UplinkLostPer(it),S_uplink(it),DownlinkLostPer(it),S_Downlink(it)] = TKG810MatlabTestCase(k,SimNum,SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara);
end