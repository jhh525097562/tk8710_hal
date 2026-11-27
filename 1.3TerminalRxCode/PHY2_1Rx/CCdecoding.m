function [vit_out] = CCdecoding( in, bitLen )

%% CC
tblen = 60;
t_p  = poly2trellis(7,[171 133]); % Define trellis.
tt_out = in;
total_N = length(in)/2;

if total_N > tblen*2
    tt_out = [tt_out tt_out];
else
    tt_out = [tt_out tt_out tt_out tt_out tt_out tt_out tt_out tt_out];
end

decoded = vitdec(tt_out + 2^(bitLen-1)-1,t_p,tblen,'trunc','soft',bitLen); % Decode.
decoded = [decoded(total_N+1:total_N+tblen) decoded(tblen+1:total_N)];

if total_N >= tblen
    vit_out = decoded;
else
    vit_out = decoded(1:total_N);
end

end