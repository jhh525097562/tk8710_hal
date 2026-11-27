function diagnose_tx_phase(referenceRoot, savedFrame, outputRoot)
% Read-only replay of the TX modulation stages from the saved reference frame.
restoredefaultpath;
addpath(referenceRoot);
for part = {'1.5Others','1.4Dump','1.3TerminalRxCode','1.1GWRxCode','1.0TerminalTxCode','1.2GWTxCode'}
    addpath(genpath(fullfile(referenceRoot,part{1})));
end
set(0,'DefaultFigureVisible','off');
s=load(savedFrame,'tx','txBits','txBaseband','weights','userWave');
mode=GetmodelparaGWTxPHY2_1(s.tx.GWTxMode,s.tx);
phase=PHY2_1Tx(s.txBits.',mode);
up=upsampling_new(phase,log2(s.tx.GWTxSampleRate/mode.FsTx));
fid=fopen(fullfile(outputRoot,'matlab_mod_phase.txt'),'w'); assert(fid>=0);
fprintf(fid,'%.17g\n',phase); fclose(fid);
fid=fopen(fullfile(outputRoot,'matlab_up_phase.txt'),'w'); assert(fid>=0);
fprintf(fid,'%.17g\n',up); fclose(fid);
info=struct('PHY2_1Tx',which('PHY2_1Tx'),'upsampling_new',which('upsampling_new'),...
    'phase_samples',numel(phase),'upsampled_samples',numel(up));
fid=fopen(fullfile(outputRoot,'matlab_stages.json'),'w'); assert(fid>=0);
fprintf(fid,'%s',jsonencode(info)); fclose(fid);
disp(info);
end
