function [U, S, V, A_tmp, V_tmp] = svd_new(in)
A = ones(8, 8) * 1e-10;
[row, col] = size(in);
A(1 : row, 1 : col) = in;
A = round(A .* 2^16) ./ 2^16;
% A(5,5) = 65535/2^16;
V=eye(8);
indexL=[1,2,3,4,5,6,7,8;1,4,3,6,5,7,2,8;1,6,3,7,5,8,2,4;1,7,3,5,4,8,2,6;1,5,3,8,4,6,2,7;1,3,6,8,4,7,2,5;1,8,6,7,4,5,2,3];
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
        [A,V, m_s]=rotaColum(A,V,indexL(inum,1),indexL(inum,2)); S_tmp = [S_tmp m_s];
        [A,V, m_s]=rotaColum(A,V,indexL(inum,3),indexL(inum,4));S_tmp = [S_tmp m_s];
        [A,V, m_s]=rotaColum(A,V,indexL(inum,5),indexL(inum,6));S_tmp = [S_tmp m_s];
        [A,V, m_s]=rotaColum(A,V,indexL(inum,7),indexL(inum,8));S_tmp = [S_tmp m_s];
        A_tmp = [A_tmp reshape(transpose(A), 1, [])];
        V_tmp = [V_tmp reshape(transpose(V), 1, [])];
%         fprintf('%d\t%d\n', iter - 1, inum - 1);
        if(iter == 2 && inum == 6)
            a = 1;
        end
        cnt = cnt + 1;
    end
end
for ind=1:8
    %s(ind)=sqrt(A1(:,ind)'*A1(:,ind));
    s(ind)=sqrt_table(A(:,ind)'*A(:,ind));
end
[Value,Index]=sort(s,'descend');
S=diag(Value);
V=V(:,Index);
V = round(V * 2^19) / 2^19;
Ai=A(:,Index);
for irow=1:8
    U(irow,:)=Ai(irow,:)./diag(S).';
end

end