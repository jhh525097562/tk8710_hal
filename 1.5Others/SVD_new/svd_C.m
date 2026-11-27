
% R0 = (orthogonalComponent)*(orthogonalComponent');
% R1 = (orthogonalComponent./64)*(orthogonalComponent'./64);
% 
% [U0,G0,V0] = svd(R0);
% [U1,G1,V1] = svd(R1);
% R = R1.*64.*64;
% [U,G,V] = svd(R);


%%
% clear all;
% close all;
% 
% save R1 R1;
% load R1 R1;
function [U2 G2 V2] = svd_C(R)
maxVal = max(max([real(R) imag(R)]));
R1 = R./maxVal;
A = ones(8, 8) * 1e-10;
[row, col] = size(R1);
A(1 : row, 1 : col) = R1;
% A(5,5) = 65535/2^16;
V2=eye(8);
indexL=[1,2,3,4,5,6,7,8;...
        1,4,3,6,5,7,2,8;...
        1,6,3,7,5,8,2,4;...
        1,7,3,5,4,8,2,6;...
        1,5,3,8,4,6,2,7;...
        1,3,6,8,4,7,2,5;...
        1,8,6,7,4,5,2,3];
num=7;
numIter=3;
A_tmp = [];
S_tmp = [];
V_tmp = [];
cnt = 0;
for iter=1:numIter
    for inum=1:num   %这个for循环是并行的WW
        if(cnt == 4)
            a = 1;
        end
        [A,V2, m_s]=rotaColumTest(A,V2,indexL(inum,1),indexL(inum,2)); S_tmp = [S_tmp m_s];
        [A,V2, m_s]=rotaColumTest(A,V2,indexL(inum,3),indexL(inum,4));S_tmp = [S_tmp m_s];
        [A,V2, m_s]=rotaColumTest(A,V2,indexL(inum,5),indexL(inum,6));S_tmp = [S_tmp m_s];
        [A,V2, m_s]=rotaColumTest(A,V2,indexL(inum,7),indexL(inum,8));S_tmp = [S_tmp m_s];
        A_tmp = [A_tmp reshape(transpose(A), 1, [])];
        V_tmp = [V_tmp reshape(transpose(V2), 1, [])];
%         fprintf('%d\t%d\n', iter - 1, inum - 1);
        if(iter == 2 && inum == 6)
            a = 1;
        end
        cnt = cnt + 1;
    end
end
for ind=1:8
    s(ind)=sqrt(A(:,ind)'*A(:,ind));
%     s(ind)=sqrt_table(A(:,ind)'*A(:,ind));
end
[Value,Index]=sort(s,'descend');
G2=diag(Value).*maxVal;
V2=V2(:,Index);

Ai=A(:,Index);
for irow=1:8
    U2(irow,:)=Ai(irow,:)./Value;
end

end
% V2=U2;
% snr_temp = [snr_lineva(V(:, 1), V2(:, 1),0) ...
% snr_lineva(V(:, 2), V2(:, 2),0) ...
% snr_lineva(V(:, 3), V2(:, 3),0) ...
% snr_lineva(V(:, 4), V2(:, 4),0) ...
% snr_lineva(V(:, 5), V2(:, 5),0) ...
% snr_lineva(V(:, 6), V2(:, 6),0) ...
% snr_lineva(V(:, 7), V2(:, 7),0) ...
% snr_lineva(V(:, 8), V2(:, 8),0)];