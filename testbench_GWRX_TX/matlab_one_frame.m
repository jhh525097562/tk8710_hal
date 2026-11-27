function matlab_one_frame(referenceRoot, outputRoot, hooksRoot, requestedUsers)
% One deterministic uplink frame, independent MATLAB RX and eight-antenna TX.
% Hooks add output arguments and run the one-user parfor loops locally.
restoredefaultpath;
if nargin<4, requestedUsers=1; end
assert(requestedUsers>=1 && requestedUsers<=128 && requestedUsers==floor(requestedUsers));
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
tm.TxUserNum = requestedUsers;
rx.GWRxUserNum = requestedUsers;
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
for uid=1:requestedUsers
    [terminalSignal(uid,:),bw(uid)] = PHY2_1TerminalTxMainCode(sim,tm,rx,frequency(uid),mode(uid),power(uid),payload(uid,:),fbit(uid,:),sbit(uid,:),imagePower(uid));
    if mod(uid,16)==0, fprintf('MATLAB_TERMINAL_TX %d/%d\n',uid,requestedUsers); end
end
[antennaInput,channel,~,timing] = UpLinkChannelModel(sim,tm,rx,ones(8,1),terminalSignal,bw(1),attenuation,1);
[users,userCount,lost,userIds,noise,inputAtCmodel] = GWPHYRx(caseNum,antennaInput,rx,1,sim.GWRxSNR);
requiredSamples = rx.GWRxFFTLen*(rx.GWRxDataWindowNum+1);
inputAtCmodel = inputAtCmodel(:,1:requiredSamples);
inputDir = fullfile(outputRoot,'Cmodel_DataIn','case95');
mkdir(inputDir);
write_iq(fullfile(inputDir,'UD_InData0.txt'),inputAtCmodel);
write_iq(fullfile(outputRoot,'matlab_rx_antenna_input.txt'),antennaInput);
payloadLength = tm.TMTxByteLen - 2*tm.TMTxCRCEn;
assert(userCount>0,'MATLAB decoded no users; input waveform was retained');
rxRecords=struct([]);
rxIds=zeros(1,userCount);
rxPayload=zeros(userCount,payloadLength);
for k=1:userCount
    rxPayload(k,:)=users(k).RxData(1:payloadLength);
    % The existing generator encodes IDs as byte1*255+byte2.
    rxIds(k)=rxPayload(k,1)*255+rxPayload(k,2);
    validId=rxIds(k)>=1 && rxIds(k)<=requestedUsers;
    payloadCorrect=validId && isequal(double(rxPayload(k,:)),double(payload(rxIds(k),1:payloadLength)));
    rxRecords(k).user_id=rxIds(k);
    rxRecords(k).payload=rxPayload(k,:);
    rxRecords(k).crc=users(k).CRC;
    rxRecords(k).frequency=users(k).Freq;
    rxRecords(k).pilot_power=users(k).RxPilotPower;
    rxRecords(k).ah_i=real(users(k).RxAH);
    rxRecords(k).ah_q=imag(users(k).RxAH);
    rxRecords(k).payload_matches_transmitted=payloadCorrect;
end
assert(numel(unique(rxIds))==userCount && all(rxIds>=1 & rxIds<=requestedUsers),'Ambiguous MATLAB RX IDs');

% Use the gateway implementations explicitly: terminal and gateway copies of
% upsampling_new differ, so path resolution must not depend on genpath order.
addpath(genpath(fullfile(referenceRoot,'1.2GWTxCode')),'-begin');
addpath(hooksRoot,'-begin');
clear upsampling_new PHY2_1Tx;
gwMode = tx.GWTxMode;
gwParameters = GetmodelparaGWTxPHY2_1(gwMode,tx);
txPayloadBank = mod(17*(0:gwParameters.Len_TxByte-3)+3+31*(0:requestedUsers-1).',256);
txPayload = txPayloadBank(rxIds,:);
txF = 0; txS = 0;
rxAH=reshape([users.RxAH],8,userCount);
[txRf,txModeParameters,txPower,txBaseband,weights,userWave] = PHYGWTx(...
    rxAH,[users.RxPilotPower],repmat(gwMode,1,userCount),[users.Freq],txPayload,txF,txS,userCount,tx,repmat(50,1,userCount),noise,calibration,1);
write_iq(fullfile(outputRoot,'matlab_tx_8ant_baseband.txt'),txBaseband);
write_iq(fullfile(outputRoot,'matlab_tx_8ant_rf.txt'),txRf);
write_iq(fullfile(outputRoot,'matlab_tx_weights.txt'),weights.');
write_iq(fullfile(outputRoot,'matlab_tx_user_wave.txt'),userWave);
txBits = zeros(requestedUsers,gwParameters.Len_TxByte*8);
for uid=1:requestedUsers
    for byte=1:size(txPayloadBank,2)
        txBits(uid,8*(byte-1)+(1:8)) = bitget(uint8(txPayloadBank(uid,byte)),8:-1:1);
    end
    txBits(uid,end-15)=txS; txBits(uid,end-14)=txF;
end
writematrix(reshape(txBits.',[],1),fullfile(outputRoot,'tx_bits.txt'),'Delimiter','space');
writematrix((1:requestedUsers).',fullfile(outputRoot,'tx_user_ids.txt'),'Delimiter','space');

% Translate actual MATLAB parameters into the existing 91-field C++ API.
[rmode,rphy] = getmodelparaGWRxPHY2_1(rx.RxMode,rx);
sampleRates = [15625 31250 62500 125000 250000 500000];
argv = zeros(1,91);
argv(1:5) = [caseNum 8 log2(rx.GWRxFFTLen/1024) rx.GWRxDataWindowNum+1 find(sampleRates==rx.GWRxSampleRate)-1];
argv(6:22) = [rx.GWRX_ANoiseThe1 rx.GWRX_ANoiseThe2 255 rx.GWRxP1_The rx.GWRX_P1_Para1 rx.GWRX_P1_Para2 rx.GWRxP2_The rx.GWRxP3_The rx.GWRxP6_The requestedUsers rx.RxMode 0 0 1 rphy.FilterLenB rphy.ifftLen rx.GWRXBFMode];
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
    'seed',1,'frames',1,'generated_user_count',requestedUsers,'tx_user_count',userCount,'rx_user_count',userCount,'rx_payload',rxPayload,...
    'rx_records',rxRecords,'rx_user_ids',rxIds,'tx_user_ids',rxIds,...
    'uplink_payload',payload(1:requestedUsers,1:payloadLength),'rx_frequency',users(1).Freq,...
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
fprintf('MATLAB_ONE_FRAME_OK generated=%d rx_users=%d bytes_per_user=%d tx_samples=%d\n',requestedUsers,userCount,payloadLength,size(txBaseband,2));
end

function write_iq(filename,value)
columns=zeros(size(value,2),size(value,1)*2);
columns(:,1:2:end)=real(value).'; columns(:,2:2:end)=imag(value).';
fid=fopen(filename,'w'); assert(fid>=0);
cleanup=onCleanup(@() fclose(fid));
fprintf(fid,[repmat('%.17g ',1,size(columns,2)-1) '%.17g\n'],columns.');
end
