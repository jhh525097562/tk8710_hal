function [U,S,V] = SVD_Test(R)
TmpMax = max([max(real(R)) max(imag(R))]);
A = R./TmpMax;
path_c = './1.5Others/SVD_new/svd';
file_in = ['./c_data/in_' num2str(0) '.txt'];
write_matrix(A, file_in);
cmd = [path_c ' ' num2str(0) ' ' num2str(8)];
system(cmd);

file_o = ['./data/case' num2str(0) '/U_o.txt']; U = read_matrix(file_o);
file_o = ['./data/case' num2str(0) '/S_o.txt']; S = read_matrix(file_o);
file_o = ['./data/case' num2str(0) '/V_o.txt']; V = read_matrix(file_o);

S = S.*TmpMax;

end