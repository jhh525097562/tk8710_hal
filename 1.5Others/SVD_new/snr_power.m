function out = snr_power(in)
%%function out = power(in)
% out = mean(abs(in).^2);
[x,y] = size(in);
in = reshape(in,1,x*y);
out = (in*in')/x/y;
return



