function   Out = DeInterLvPunch(In,addLen,SCodeIn)
%% 解交织
%%%%%%%%%%%%%%%%2201
InterLvIndex = [22,10,16,4,19,7,13,1,21,9,15,3,18,6,12,0,2,14,8,20,5,17,11,23] + 1;
Column = 24;
InTemp = [In zeros(1,addLen)];
row = ceil( length(InTemp)/Column );
count = 0;
OutTemp = zeros(1,length(InTemp));
for i = 1:Column
    ii = InterLvIndex(i);
    for j = 1: row
        tempIndex = (j-1) * Column + ii;
        if  tempIndex <= length(InTemp)
            count = count + 1;
            OutTemp( tempIndex ) = InTemp ( count );
%             tempIndexSave(count) = tempIndex;
        end
    end
end

SCcode = [SCodeIn SCodeIn];
Out = OutTemp.*(1-2*SCcode(1:length(OutTemp)));
%%%%%%%%%%%%%%%8610
% InTemp = [In zeros(1,addLen)];
% col = 24;  % 列
% row = ceil( length(InTemp)/col );  % 行
% 
% IndexIn = [1:col*row];
% IndexInReshape = reshape(IndexIn, col,row)';
% intrlvIndex = reshape(IndexInReshape,1,[]);
% 
% Out = zeros(length(intrlvIndex),1);
% for ii = 1:length(intrlvIndex)
%     Out( intrlvIndex(ii) ) =  In( ii );
% end
% SCode = SCodeIn;
% Out = Out.*(1-2*SCode.');
% Out = Out.';
end