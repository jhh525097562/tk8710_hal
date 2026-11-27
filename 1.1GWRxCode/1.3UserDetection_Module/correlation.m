% function out = correlation(v1,v2,conju);
% 
% if conju
%     v1 = v1';
% else
%     m = size(v1,1);
%     v1 = reshape(v1,1,m);
% end
% 
% m = size(v2,2);
% out = zeros(1,m);
% for i = 1:m
%    out(i) = abs(v1*v2(:,i))/norm(v1)/norm(v2(:,i)); 
% end

% function out = correlation(v1,v2,Len,Size)
% 
% v1 = v1(1:Len,1)';
% 
% out = zeros(1,Size);
% for i = 1:Size
%    out(i) = abs(v1*v2(1:Len,i))/norm(v1(1:Len))/norm(v2(1:Len,i)); 
% end

function out = correlation(a,b,Len,Size,APreambleFft1,APreambleFft0,flag)

v0 = APreambleFft1(1:Len,a);
if flag==0
    v2 = APreambleFft0(1:Len,b(1:Size));
else
    v2 = APreambleFft1(1:Len,b(1:Size));
end

v1 = v0';
out = zeros(1,Size);
for i = 1:Size
   out(i) = abs(v1*v2(1:Len,i))/norm(v1(1:Len))/norm(v2(1:Len,i)); 
end