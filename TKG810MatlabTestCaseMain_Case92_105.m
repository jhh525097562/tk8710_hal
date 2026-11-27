clc;
clear all;
close all;

addpath(genpath('1.5Others'));
addpath(genpath('1.4Dump'));
addpath(genpath('1.3TerminalRxCode'));
addpath(genpath('1.2GWTxCode'));
addpath(genpath('1.1GWRxCode'));
addpath(genpath('1.0TerminalTxCode'));
addpath(genpath('Log'));
addpath(genpath('data'));

TestCasePara = xlsread('TestCaseListNew3.xlsx');
% TestCaseList = [173 174 175 176 177 221 225 227 232 233 238 239 244];%138
% TestCaseList = [28:37];%138
TestCaseList = [35];%138
% TestCaseList = [111:119];%138
SimNum = 100;
% parfor it = 1:length(TestCaseList)
for it = 1:length(TestCaseList)
    k = TestCaseList(it);
    %SysParaIn
    [SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara] = TestCaseListParaSet(TestCasePara(k,:));

    [UplinkLostPer(it),S_uplink(it),DownlinkLostPer(it),S_Downlink(it)] = TKG810MatlabTestCase(k,SimNum,SimPara,TMTXPara,GWRXPara,GWTXPara,TMRXPara);
end