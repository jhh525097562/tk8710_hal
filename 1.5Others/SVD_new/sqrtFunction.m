function rootVal=sqrtFunction(val)
% disp(val);
% val = round(val * 2^11) / 2^11;% * 2^-1) / 2^-1;
length=val;
wide=0.0;
iterNum=0;
while(abs(length-wide)>0.000001)
    wide=length;
    length=0.5*(length+val/length);
    iterNum=iterNum+1;
end
rootVal=length;
end