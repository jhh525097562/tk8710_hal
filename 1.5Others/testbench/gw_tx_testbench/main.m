clc;
% clear;

addpath '../../../1.0TerminalTxCode/'
addpath '../../../1.1GWRxCode/'
addpath '../../../1.2GWTxCode/'
addpath '../../../1.3TerminalRxCode/'
addpath '../../../1.4Dump/'
addpath '../../../1.5Others/'
file_c = '/home/hrren/work/K2301/alg/Codeblocks/tx_bf/tx_bf_tb/bin/Release/tx_bf_tb';
case_list = table2array(readtable('/home/hrren/work/K2301/alg/c_model/tx_main/testbench.csv'));
for ck0 = 1 : 1 : 11
    [GWTXAH,GWRxPilotPower,GWTXFreq,ANoise,CalibrateFactor, GWRxUserNum, GWTXMode, GWTxData, Fbits, Sbits, GWTXPower, GWTXPara] = gwtx_load(ck0 - 1, case_list(ck0, 2));
    [GW_Tx_MutiAntennaSignal_o,ModeParaDown,PowerTxdBm] = PHYGWTx_testbench(GWTXAH,GWRxPilotPower,GWTXMode,GWTXFreq,GWTxData,Fbits,Sbits,GWRxUserNum,GWTXPara,GWTXPower,ANoise,CalibrateFactor,1);
    cmd = [file_c ' ' num2str(case_list(ck0, 1)) ' ' num2str(case_list(ck0, 2)) ...
            ' ' num2str(case_list(ck0, 3)) ' ' num2str(case_list(ck0, 4)) ...
            ' ' num2str(case_list(ck0, 5)) ' ' num2str(case_list(ck0, 6)) ...
            ' ' num2str(case_list(ck0, 7)) ' ' num2str(case_list(ck0, 8)) ...
            ' ' num2str(case_list(ck0, 9))];
    system(cmd);
    filename = ['./data/case' num2str(ck0 - 1) '/GW_Tx_out.txt'];
    data = load_file(filename, 'txt');
    snr(ck0) = snr_lineva(GW_Tx_MutiAntennaSignal_o, data, 0);
    disp(snr);
end