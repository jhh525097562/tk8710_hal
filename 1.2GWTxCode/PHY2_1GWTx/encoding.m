function [enc_out1,enc_out2] = encoding(in,FskEncodingbitsLen)

% 卷积码
% 第一段编码
enc_out1 = CCEncoder(in(1:FskEncodingbitsLen));
% 第二段编码
enc_out2 = CCEncoder(in(FskEncodingbitsLen+1:end));

end