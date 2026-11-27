function out = upsampling_new(in, m)
intmp = [in.'  in(end) + in(end) - in(end-1)];
filter_o = intmp;
for ck0 = 1 : 1 : m
    for ck1 = 1 : length(filter_o) - 1
        temp(2 * ck1 - 1) = filter_o(ck1);
        temp(2 * ck1) = add(filter_o(ck1), filter_o(ck1 + 1));
    end
    temp(2 * length(filter_o) - 1) = filter_o(end); 
    filter_o = temp;
end
out = filter_o;
end

function out = add(in_1, in_2)
if((in_1 - in_2 > 1))
    out = in_1 + in_2 + 2.0 ;
elseif((in_2 - in_1 > 1))
    out = in_1 + in_2 - 2.0 ;
else
    out = in_1 + in_2;
end

out = out / 2;
if(out > 1)
    out = out - 2;
elseif(out < -1)
    out = out + 2;
end
    
end