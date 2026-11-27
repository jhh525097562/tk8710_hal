function y = h_func(x1,x2);

m = size(x1,2);
y =zeros(1,m);
for i=1:m
    if x1(i) > x2
        y(i) = 1;
    else
        y(i) = 0;
    end
end

