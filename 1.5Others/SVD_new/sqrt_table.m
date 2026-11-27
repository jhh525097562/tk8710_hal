function rootVal=sqrt_table(val)
% disp(val);
% val = round(val * 2^11) / 2^11;% * 2^-1) / 2^-1;
val_tmp = val / 4096;
shift = log2(val_tmp) + 1;
val_n = round(val / 2^shift);
out = sqrt(val_n) * sqrt(2^shift);
rootVal=out;
end