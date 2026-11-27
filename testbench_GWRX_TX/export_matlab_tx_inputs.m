function export_matlab_tx_inputs(savedFrame,outputRoot)
% Export the exact PHYGWTx arguments from the already generated frame.
s=load(savedFrame,'users','noise','calibration','tx','txBits','argv','txPayload','txBaseband','weights');
count=numel(s.users);
assert(count>=1 && count<=128 && numel(s.noise)==8 && numel(s.calibration)==8);
fid=fopen(fullfile(outputRoot,'matlab_tx_inputs.txt'),'w');assert(fid>=0);
fprintf(fid,'MATLAB_TX_V1 %d 8\n',count);
fprintf(fid,'%.17g ',s.noise);fprintf(fid,'\n');
cal=zeros(1,16);cal(1:2:end)=real(s.calibration);cal(2:2:end)=imag(s.calibration);
fprintf(fid,'%.17g ',cal);fprintf(fid,'\n');
ids=zeros(count,1);
for k=1:count
    ids(k)=s.users(k).RxData(1)*255+s.users(k).RxData(2);
    ah=zeros(1,16);ah(1:2:end)=real(s.users(k).RxAH);ah(2:2:end)=imag(s.users(k).RxAH);
    % matlab_one_frame calls PHYGWTx with per-user TX power 50.
    fprintf(fid,'%d %d %.17g %.17g %.17g ',ids(k),s.tx.GWTxMode,s.users(k).Freq,s.users(k).RxPilotPower,50);
    fprintf(fid,'%.17g ',ah);fprintf(fid,'\n');
end
fclose(fid);
assert(numel(unique(ids))==count);
bits=s.txBits(ids,:);
% Check the bit bank against the payload that was actually passed to PHYGWTx.
for k=1:count
    for byte=1:size(s.txPayload,2)
        assert(isequal(bits(k,(byte-1)*8+(1:8)),double(bitget(uint8(s.txPayload(k,byte)),8:-1:1))));
    end
end
writematrix(reshape(bits.',[],1),fullfile(outputRoot,'tx_bits.txt'),'Delimiter','space');
writematrix(ids,fullfile(outputRoot,'tx_user_ids.txt'),'Delimiter','space');
fid=fopen(fullfile(outputRoot,'cmodel_args.txt'),'w');assert(fid>=0);
fprintf(fid,'%.17g ',s.argv);fprintf(fid,'\n');fclose(fid);
metadata=struct('saved_frame',savedFrame,'tx_user_count',count,'tx_user_ids',ids,...
    'fields','AH, noise power, frequency, pilot power, mode, per-user TX power, calibration, PHY TX bits, all 91 case parameters',...
    'matlab_tx_samples',size(s.txBaseband,2),'tx_bits_per_user',size(bits,2));
fid=fopen(fullfile(outputRoot,'export.json'),'w');assert(fid>=0);fprintf(fid,'%s',jsonencode(metadata));fclose(fid);
fprintf('MATLAB_TX_EXPORT_OK users=%d samples=%d\n',count,size(s.txBaseband,2));
end
