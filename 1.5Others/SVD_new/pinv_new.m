function X = pinv_new(A)
%PINV   Pseudoinverse.
%   X = PINV(A) produces a matrix X of the same dimensions
%   as A' so that A*X*A = A, X*A*X = X and A*X and X*A
%   are Hermitian. The computation is based on SVD(A) and any
%   singular values less than a tolerance are treated as zero.
%
%   PINV(A,TOL) treats all singular values of A that are less than TOL as
%   zero. By default, TOL = max(size(A)) * eps(norm(A)).
%
%   Class support for input A:
%      float: double, single
%
%   See also RANK.

%   Copyright 1984-2015 The MathWorks, Inc.

% [U,S,V] = svd_new(A);
% path_c = './1.5Others/SVD_new/svd';

%%
% path_c = './1.5Others/SVD_new/svd';
path_c = './1.5Others/SVD_new/svd_tx';
file_in = ['./c_data/in_' num2str(0) '.txt'];
write_matrix(A, file_in);
cmd = [path_c ' ' num2str(0) ' ' num2str(8)];
system(cmd);

file_o = ['./data/case' num2str(0) '/U_o.txt']; U = read_matrix(file_o);
file_o = ['./data/case' num2str(0) '/S_o.txt']; S = read_matrix(file_o);
file_o = ['./data/case' num2str(0) '/V_o.txt']; V = read_matrix(file_o);
%%

% s = diag(S);
% s = 1./s(:);
% X = (V.*s.')*U';

% [U, S, V, A_tmp, V_tmp] = svd_new(A);

num = size(S,1);
countNoZero = 0;
tol = 0;
for i=1:7
%     if S(i,i)/S(i+1,i+1) > 100
%         tol = S(i+1,i+1);
%         break;
%     end
    if S(i,i)/S(i+1,i+1) > 10^2
        tol = S(i+1,i+1);
        break;
    end
end
for i=1:num
    if abs(S(i,i))>tol
        countNoZero = countNoZero + 1;
    end
end
tempMat = zeros(num,num);
for i=1:countNoZero
    tempMat(i,i) = 1/S(i,i);
end
X = V*tempMat*U';
