function out = inverse_sqrt(in)
threehalfs = 1.5;

x2 = in * 0.5;
y = in;
i = num2hex(single(y));
i = 1597463007 - (hex2dec(i) / 2);
M = mod(i, 2^23);
E = floor(i / 2^23);
y = (1 + M / 2^23) * 2^(E - 127);
y = y * (threehalfs - (x2 * y * y));
y = y * (threehalfs - (x2 * y * y));
out = y * (threehalfs - (x2 * y * y));
end