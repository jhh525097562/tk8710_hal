function [U_est,S_est,V_est] = SVD_Single_Jacobi_Algorithm_Complex(A1,AntennaNum,Num)


A=[real(A1),-imag(A1);imag(A1),real(A1)];
n = AntennaNum*2;
U=eye(n);
V=eye(n);

numIter=Num;
for iterNum=1:numIter
    for row=1:n-1
        for col=row+1:n
            al =0; b =0;c =0;L=0; t =0; cs=0; sn =0; tmp =0; signn = 0;
            b=A(:,col).'*A(:,col);
            a1=A(:,row).'*A(:,row);
            c=A(:,col).'*A(:,row);
            if abs(c)<1e-18
                break
            end
            % 计算jacobi旋转因子
            L=(b-a1)/(2*c);
            t=sign(L)/(abs(L)+sqrt(1+L^2));
            cs=1/sqrt(1+t^2);
            sn=cs*t;
            %
            temp=A(:,row);
            A(:,row)=cs*temp-sn*A(:,col);
            A(:,col)=sn*temp+cs*A(:,col);
            % 更新右奇异矩阵V
            tempV=V(:,row);
            V(:,row)=cs*tempV-sn*V(:,col);
            V(:,col)=sn*tempV+cs*V(:,col);
        end
    end
end
% 寻找奇异值矩阵
for ind=1:n
    s(ind)=sqrt(A(:,ind).'*A(:,ind));
end
[Value,Index]=sort(s,'descend');
Si=diag(Value);
Vi=V(:,Index);
Ai=A(:,Index);
for irow=1:n
    Ui(irow,:)=Ai(irow,:)./diag(Si).';
end
U_est=Ui(1:AntennaNum,1:2:n)+1i*Ui(AntennaNum+1:end,1:2:n);
V_est=Vi(1:AntennaNum,1:2:n)+1i*Vi(AntennaNum+1:end,1:2:n);
S_est=diag(Value(1:2:end));
end