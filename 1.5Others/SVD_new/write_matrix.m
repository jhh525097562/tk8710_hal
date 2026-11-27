function write_matrix(in, filename)
in = transpose(in);
in = reshape(in, 1, []);
write2data(in, filename, 'txt');
end