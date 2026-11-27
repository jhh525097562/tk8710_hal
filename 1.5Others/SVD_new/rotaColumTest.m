function [A,V, s]=rotaColumTest(Amat,Vmat,colK,colL)
A1=Amat;%20.10
V=Vmat;
K=colK;
L=colL;
wK=A1(:,K)'*A1(:,K);%32.20
wL=A1(:,L)'*A1(:,L);%32.20
xiu=A1(:,K)'*A1(:,L);%32.20
% xiuAbs=sqrt_table((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));
% xiuAbs=sqrtFunction((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));  
% xiuAbs=abs(xiu);   %
% xiuTmp=xiu/xiuAbs; xiuTmp = round(xiuTmp * 2^18) / 2^18;
abs_xiu = abs(xiu)^2;
abs_xiu_sqrt = sqrt(abs_xiu);%32.20
xiuTmp = xiu / abs_xiu_sqrt;%19.1
sqrt_temp = 1 / abs_xiu_sqrt;
% sqrt_temp = inverse_sqrt((real(xiu)*real(xiu) + imag(xiu) * imag(xiu))); sqrt_temp = round(sqrt_temp * 2^15) / 2^15;
% xiuTmp = xiu * sqrt_temp;
QKL=wK-wL;%32.20
if abs(QKL)<(16/2^24)%1e-4
    QKL=16/2^24;%1e-4
end
%t=2*xiuAbs/(QKL+sign(QKL)*sqrt(QKL*QKL+4*xiuAbs*xiuAbs));
% tmp=QKL/xiuAbs;
tmp=QKL* sqrt_temp;%inverse_sqrt((real(xiu)*real(xiu) + imag(xiu) * imag(xiu)));34.20
tmp_2_4 = tmp*tmp+4;%34.30
sqrt_temp = sqrt(tmp_2_4);%24.14
t=2/(tmp+sign(QKL) * sqrt_temp);%21.1

% QKL
% sqrt_temp = 1 / inverse_sqrt(tmp*tmp+4);
% t=2/(tmp+sign(QKL) * sqrt_temp);
t_2_1 = 1+t*t;%24.3
t_2_1_sqrt = sqrt(t_2_1);%12.2
c=1/t_2_1_sqrt;%16.1
s=t*c;%28.1
% disp(c);
% disp(s);
tmpA=A1(:,K);
temp_1 = c*xiuTmp*tmpA;
temp_2 = s*A1(:,L);%32.8
temp = temp_1 + temp_2;
A1(:,K)= temp;
temp_1 = -s*xiuTmp*tmpA;
temp_2 = c*A1(:,L);%23.4
temp = temp_1 + temp_2;
A1(:,L)=temp;

tmpU=V(:,K);
% temp = c*xiuTmp*tmpU+s*V(:,L);temp = round(temp * 2^19) / 2^19;
temp_1 = c*xiuTmp*tmpU;
temp_2 = s*V(:,L);
temp = temp_1 + temp_2;
V(:,K)=temp;

% temp = -s*xiuTmp*tmpU+c*V(:,L);temp = round(temp * 2^19) / 2^19;
temp_1 = -s*xiuTmp*tmpU;
temp_2 = c*V(:,L);
temp = temp_1 + temp_2;
V(:,L)= temp;
A=A1;
% max(max(V))
% fprintf('%f\n', A1(8,L));
end
