function   cc_out = CCEncoder(cc_in)

t_p  = poly2trellis(7,[171 133]); % Define trellis.
circle_tail = 6;
convenc_state = sum(cc_in(end-circle_tail+1:end) .* 2.^(0 : 5));
[cc_out] = convenc([cc_in(end-circle_tail+1:end) cc_in],t_p,convenc_state);
cc_out   = cc_out(circle_tail*2+1:circle_tail*2+length(cc_in)*2);

end