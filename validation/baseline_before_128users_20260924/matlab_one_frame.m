function matlab_one_frame(referenceRoot, outputRoot, hooksRoot)
% One deterministic uplink frame, independent MATLAB RX and eight-antenna TX.
% Hooks add output arguments and run the one-user parfor loops locally.
restoredefaultpath;
for name = {'1.5Others','1.4Dump','1.3TerminalRxCode','1.2GWTxCode','1.1GWRxCode','1.0TerminalTxCode','1.6GWRecipracalCode'}
    addpath(genpath(fullfile(referenceRoot,name{1})));
end
addpath(referenceRoot);
addpath(hooksRoot,'-begin');
set(0,'DefaultFigureVisible','off');
rng(1,'twister');
caseNum = 95;
% xlsread's numeric output matches the original project driver (text case
% labels are excluded; readmatrix's automatic range includes that column).
table = xlsread(fullfile(referenceRoot,'TestCaseListNew3.xlsx'));
[sim,tm,rx,tx,terminalRx] = TestCaseListParaSet(table(caseNum,:));
assert(rx.AntennaNum==8 && rx.GWRxFFTLen==8192 && rx.RxMode==4);
tm.TxUserNum = 1;
rx.GWRxUserNum = 1;
tm.TMTxBlockNum = 1;
rx.GWRxBlockNum = 1;
tx.GWTxBlockNum = 1;
sim.GWRxSNR = 30;
sim.CFOFreqFlag = 0;
sim.DriftPerSecFlag = 0;
sim.TimingOffsetFlag = 0;
sim.GWRXRFDelayFlag = 0;
sim.Periodic_InterferenceFlag = 0;
sim.SingleToneInterferenceFlag = 0;
sim.Tx_Image_InterferenceFlag = 0;
sim.RecipracalCaliFlag = 0;
tx.RecipracalCaliFlag = 0;
tx.GWTxPowerFlag = 1;
tx.GWTxAHSelectFlag = 0;
tx.GWTxToneEn = 0;
calibration = ones(1,8);
[frequency,mode,power,payload,fbit,sbit,imagePower,~,attenuation] = ...
    SimCode_TerminalTxParaSet(sim,tm,rx,tx,1);
[terminalSignal,bw] = PHY2_1TerminalTxMainCode(sim,tm,rx,frequency(1),mode(1),power(1),payload(1,:),fbit(1,:),sbit(1,:),imagePower(1));
[antennaInput,channel,~,timing] = UpLinkChannelModel(sim,tm,rx,ones(8,1),terminalSignal,bw(1),attenuation,1);
[users,userCount,lost,userIds,noise,inputAtCmodel] = GWPHYRx(caseNum,antennaInput,rx,1,sim.GWRxSNR);
assert(userCount==1,'MATLAB failed to decode the one generated user');
requiredSamples = rx.GWRxFFTLen*(rx.GWRxDataWindowNum+1);
inputAtCmodel = inputAtCmodel(:,1:requiredSamples);
inputDir = fullfile(outputRoot,'Cmodel_DataIn','case95');
mkdir(inputDir);
write_iq(fullfile(inputDir,'UD_InData0.txt'),inputAtCmodel);
write_iq(fullfile(outputRoot,'matlab_rx_antenna_input.txt'),antennaInput);
payloadLength = tm.TMTxByteLen - 2*tm.TMTxCRCEn;
rxPayload = users(1).RxData(1:payloadLength);
assert(isequal(double(rxPayload(:)),double(payload(1,1:numel(rxPayload)).')),'MATLAB RX differs from generated uplink payload');

% Use the gateway implementations explicitly: terminal and gateway copies of
% upsampling_new differ, so path resolution must not depend on genpath order.
addpath(genpath(fullfile(referenceRoot,'1.2GWTxCode')),'-begin');
addpath(hooksRoot,'-begin');
clear upsampling_new PHY2_1Tx;
gwMode = tx.GWTxMode;
gwParameters = GetmodelparaGWTxPHY2_1(gwMode,tx);
txPayload = mod(17*(0:gwParameters.Len_TxByte-3)+3,256);
txF = 0; txS = 0;
[txRf,txModeParameters,txPower,txBaseband,weights,userWave] = PHYGWTx(...
    users(1).RxAH,users(1).RxPilotPower,gwMode,users(1).Freq,txPayload,txF,txS,1,tx,50,noise,calibration,1);
write_iq(fullfile(outputRoot,'matlab_tx_8ant_baseband.txt'),txBaseband);
write_iq(fullfile(outputRoot,'matlab_tx_8ant_rf.txt'),txRf);
write_iq(fullfile(outputRoot,'matlab_tx_weights.txt'),weights.');
write_iq(fullfile(outputRoot,'matlab_tx_user_wave.txt'),userWave);
txBits = zeros(1,gwParameters.Len_TxByte*8);
for byte=1:numel(txPayload)
    txBits(8*(byte-1)+(1:8)) = bitget(uint8(txPayload(byte)),8:-1:1);
end
txBits(end-15) = txS; txBits(end-14) = txF;
writematrix(txBits.',fullfile(outputRoot,'tx_bits.txt'),'Delimiter','space');

% Translate actual MATLAB parameters into the existing 91-field C++ API.
[rmode,rphy] = getmodelparaGWRxPHY2_1(rx.RxMode,rx);
sampleRates = [15625 31250 62500 125000 250000 500000];
argv = zeros(1,91);
argv(1:5) = [caseNum 8 log2(rx.GWRxFFTLen/1024) rx.GWRxDataWindowNum+1 find(sampleRates==rx.GWRxSampleRate)-1];
argv(6:22) = [rx.GWRX_ANoiseThe1 rx.GWRX_ANoiseThe2 255 rx.GWRxP1_The rx.GWRX_P1_Para1 rx.GWRX_P1_Para2 rx.GWRxP2_The rx.GWRxP3_The rx.GWRxP6_The 1 rx.RxMode 0 0 1 rphy.FilterLenB rphy.ifftLen rx.GWRXBFMode];
assert(rmode.sync_FComp_Num==0 && rmode.sync_FDComp_Num==0);
argv(23:38) = [rmode.TOScope rmode.FsRx 1 0 0 rmode.DSSS 1 0 0 1 rmode.SymNumData 0 0 rmode.data_FDComp_Num rmode.data_FDComp_Delta rmode.crc14_en];
argv(39:43) = [0 gwMode 0 0 0];
argv(44:2:59) = real(calibration);
argv(45:2:59) = imag(calibration);
argv(60:66) = [tx.GWTxPowerFlag tx.RecipracalCaliFlag 0 150 10 15 20];
up = log2(tx.GWTxSampleRate/gwParameters.FsTx);
prefix = [40 40 40 48 96 160 192 256 256 256];
% Legacy argv[67] is converted to a phase-input prefix by division in C++.
argv(67) = prefix(gwMode-10)*(1e6/2^(up+1));
argv(68:73) = [0 gwParameters.CodeRate gwParameters.crc14_en gwParameters.DSSS gwParameters.TransRatio 1];
argv(90:91) = [gwParameters.Len_TxByte*8/gwParameters.CodeRate/4 up];
fid=fopen(fullfile(outputRoot,'cmodel_args.txt'),'w');
assert(fid>=0); fprintf(fid,'%.17g ',argv); fprintf(fid,'\n'); fclose(fid);
metadata = struct('matlab_version',version,'reference_root',referenceRoot,'case',caseNum,...
    'seed',1,'frames',1,'tx_user_count',1,'rx_user_count',userCount,'rx_payload',rxPayload,...
    'uplink_payload',payload(1,1:numel(rxPayload)),'rx_frequency',users(1).Freq,...
    'rx_pilot_power',users(1).RxPilotPower,'rx_crc',users(1).CRC,...
    'rx_ah_i',real(users(1).RxAH),'rx_ah_q',imag(users(1).RxAH),...
    'tx_payload',txPayload,'tx_baseband_rate',tx.GWTxSampleRate,'tx_rf_rate',tx.RFSampleRate,...
    'tx_baseband_samples',size(txBaseband,2),'tx_rf_samples',size(txRf,2),...
    'cmodel_tx_scale',2^23,'sim',sim,'tm',tm,'rx',rx,'tx',tx,...
    'functions',struct('GWPHYRx',which('GWPHYRx'),'PHYGWTx',which('PHYGWTx'),...
    'PHY2_1Tx',which('PHY2_1Tx'),'upsampling_new',which('upsampling_new')));
fid=fopen(fullfile(outputRoot,'matlab_results.json'),'w');
assert(fid>=0); fprintf(fid,'%s',jsonencode(metadata)); fclose(fid);
save(fullfile(outputRoot,'matlab_one_frame.mat'),'sim','tm','rx','tx','terminalRx','users','noise',...
    'inputAtCmodel','antennaInput','terminalSignal','channel','timing','calibration','payload','txPayload',...
    'txBits','txRf','txBaseband','weights','userWave','txModeParameters','txPower','argv','userIds','lost');
fprintf('MATLAB_ONE_FRAME_OK rx_users=%d bytes=%d tx_samples=%d\n',userCount,numel(rxPayload),size(txBaseband,2));
end

function write_iq(filename,value)
columns=zeros(size(value,2),size(value,1)*2);
columns(:,1:2:end)=real(value).'; columns(:,2:2:end)=imag(value).';
fid=fopen(filename,'w'); assert(fid>=0);
cleanup=onCleanup(@() fclose(fid));
fprintf(fid,[repmat('%.17g ',1,size(columns,2)-1) '%.17g\n'],columns.');
end
