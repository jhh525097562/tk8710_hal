function out = read_matrix(filename)
out = load_file(filename, 'txt');
out = transpose(out);
len = length(out);
row = sqrt(len);
out = reshape(out, row, []);
out = transpose(out);
end