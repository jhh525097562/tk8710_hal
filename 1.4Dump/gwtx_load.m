function [GWTXAH,GWRxPilotPower,GWTXFreq,ANoise,CalibrateFactor, GWRxUserNum, GWTXMode, GWTxData, Fbits1, Sbits1, GWTXPower, GWTXPara] = gwtx_load(case_num, mode)
folder_name = ['/home/data/K2301/tx_bf/c_data/case' num2str(case_num) '/'];

filename = [folder_name 'in_2.txt'];
GWTXAH = load_file(filename, 'txt');
GWTXAH = reshape(transpose(GWTXAH), 8, []);

filename = [folder_name 'in_3.txt'];
ANoise_o = load_file(filename, 'txt');
ANoise = reshape(transpose(ANoise_o), 8, []);

filename = [folder_name 'in_4.txt'];
CalibrateFactor = load_file(filename, 'txt');

filename = [folder_name 'in_5.txt'];
GWTXFreq = load_file(filename, 'txt');

filename = [folder_name 'in_6.txt'];
GWRxPilotPower = load_file(filename, 'txt');

[~, GWRxUserNum] = size(GWTXAH);
GWTXMode = ones(1, GWRxUserNum) * mode;

load([folder_name 'GWTxData.mat']);

load([folder_name 'Fbits1.mat']);

load([folder_name 'Sbits1.mat']);

GWTXPower = 50 * ones(1, GWRxUserNum);

load([folder_name 'GWTXPara.mat']);
end