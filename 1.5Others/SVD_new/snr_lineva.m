function [snr, coff]=snr_lineva(x,y,mode)
% function [snr, coff]=snr_lineva(x,y)
% the line for noise evaluation
% snr is 10*log10(coff*x/noise)   x in    and    y out;
% just think y=x*coff+noise
x=reshape(x,1,size(x,1)*size(x,2));
y=reshape(y,1,size(y,1)*size(y,2));
len=min(length(x),length(y));
x=x(1:len);y=y(1:len);
if(mode==0)
    coff=(y*x')/(x*x');
else
    coff = 1;
end
 %%% just think the channel as  rd=(td+d_nos)*coff_d, finding 'coff_d'
snr=10*log10(snr_power(x*coff)/snr_power(y-x*coff)); %%%% this for caculating 'snr' 

