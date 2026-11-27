function [A,V, s]=rotaColum(Amat,Vmat,colK,colL)
A1=Amat;
V=Vmat;
K=colK;
L=colL;
wK=A1(:,K)'*A1(:,K); wK = round(wK * 2^23) / 2^23; 
wL=A1(:,L)'*A1(:,L); wL = round(wL * 2^23) / 2^23; 
xiu=A1(:,K)'*A1(:,L); xiu = round(xiu * 2^16) / 2^16; 
% xiuAbs=sqrt_table((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));
% xiuAbs=sqrtFunction((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));  
% xiuAbs=abs(xiu);   %
% xiuTmp=xiu/xiuAbs; xiuTmp = round(xiuTmp * 2^18) / 2^18;
abs_xiu = abs(xiu)^2;
abs_xiu = round(abs_xiu * 2^33) / 2^33;
abs_xiu_sqrt = sqrt(abs_xiu);
abs_xiu_sqrt = round(abs_xiu_sqrt * 2^16) / 2^16;
xiuTmp = xiu / abs_xiu_sqrt;
sqrt_temp = 1 / abs_xiu_sqrt;
% sqrt_temp = inverse_sqrt((real(xiu)*real(xiu) + imag(xiu) * imag(xiu))); sqrt_temp = round(sqrt_temp * 2^15) / 2^15;
% xiuTmp = xiu * sqrt_temp;
xiuTmp = round(xiuTmp * 2^18) / 2^18;% xiuTmp = fix_set(xiuTmp, 18, 1);
QKL=wK-wL;
if abs(QKL)<1e-4
    QKL=1e-4;
end
QKL = round(QKL * 2^25) / 2^25;
%t=2*xiuAbs/(QKL+sign(QKL)*sqrt(QKL*QKL+4*xiuAbs*xiuAbs));
% tmp=QKL/xiuAbs;
tmp=QKL* sqrt_temp;tmp = round(tmp*2^18) / 2^18;%inverse_sqrt((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));
tmp_2_4 = tmp*tmp+4;
tmp_2_4 = round(tmp_2_4 * 2^14) / 2^14;
sqrt_temp = sqrt(tmp_2_4); sqrt_temp = round(sqrt_temp * 2^14) / 2^14;
t=2/(tmp+sign(QKL) * sqrt_temp);
t = round(t * 2^24) / 2^24;

% sqrt_temp = 1 / inverse_sqrt(tmp*tmp+4);
% t=2/(tmp+sign(QKL) * sqrt_temp);
t_2_1 = 1+t*t;  t_2_1 = round(t_2_1 * 2^26) / 2^26;
t_2_1_sqrt = sqrt(t_2_1); t_2_1_sqrt = round(t_2_1_sqrt * 2^24) / 2^24;
c=1/t_2_1_sqrt; c= round(c * 2^23) / 2^23;c = fix_set(c, 23, 1);
s=t*c; s= round(s * 2^27) / 2^27;
% disp(s);
tmpA=A1(:,K);
temp_1 = c*xiuTmp*tmpA; temp_1 = round(temp_1 * 2^24) / 2^24;
temp_2 = s*A1(:,L); temp_1 = round(temp_1 * 2^24) / 2^24;
temp = temp_1 + temp_2; temp = round(temp * 2^24) / 2^24;
A1(:,K)= temp;
temp_1 = -s*xiuTmp*tmpA; temp_1 = round(temp_1 * 2^24) / 2^24;
temp_2 = c*A1(:,L);temp_2 = round(temp_2 * 2^24) / 2^24;
temp = temp_1 + temp_2;temp = round(temp * 2^24) / 2^24;
A1(:,L)=temp;

tmpU=V(:,K);
% temp = c*xiuTmp*tmpU+s*V(:,L);temp = round(temp * 2^19) / 2^19;
temp_1 = c*xiuTmp*tmpU; temp_1 = round(temp_1 * 2^19) / 2^19;
temp_2 = s*V(:,L); temp_2 = round(temp_2 * 2^19) / 2^19;
temp = temp_1 + temp_2; temp = round(temp * 2^19) / 2^19;
V(:,K)=temp;

% temp = -s*xiuTmp*tmpU+c*V(:,L);temp = round(temp * 2^19) / 2^19;
temp_1 = -s*xiuTmp*tmpU;temp_1 = round(temp_1 * 2^19) / 2^19;
temp_2 = c*V(:,L);temp_2 = round(temp_2 * 2^19) / 2^19;
temp = temp_1 + temp_2;temp = round(temp * 2^19) / 2^19;
V(:,L)= temp;
A=A1;
A = round(A * 2^24) / 2^24;
V = round(V * 2^19) / 2^19;

% fprintf('%f\n', A1(8,L));
end

function out = fix_set(in, wl, il)
if(abs(real(in)) == 1)
    I = sign(real(in)) * abs(real(in)) * (2^wl - 1) / 2^wl;
else
    I = real(in);
end
if(abs(imag(in)) == 1)
    Q = sign(imag(in)) * abs(imag(in)) * (2^wl - 1) / 2^wl;
else
    Q = imag(in);
end
out = I + Q * sqrt(-1);
end