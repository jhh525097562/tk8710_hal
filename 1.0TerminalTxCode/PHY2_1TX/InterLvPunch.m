function   Out =  InterLvPunch(In,outLen,SCodeIn)

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%2201
SCcode = [SCodeIn SCodeIn];
Inxor = xor(In,SCcode(1:length(In)));


InterLvIndex = [22,10,16,4,19,7,13,1,21,9,15,3,18,6,12,0,2,14,8,20,5,17,11,23] + 1;
Column = 24;
row = ceil( length(Inxor)/Column );
count = 0;
Outtemp = zeros(1, length(Inxor));
for i = 1:Column
    ii = InterLvIndex(i);
    for j = 1: row
        tempIndex = (j-1) * Column + ii;
        if  tempIndex <= length(Inxor)
            count = count + 1;
            Outtemp( count ) = Inxor ( tempIndex );
%             tempIndexSave(count) = tempIndex;
        end
    end
end
Out = Outtemp(1:outLen);
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%8610
% SCode = SCodeIn;
% In1 = xor(In,SCode);
% 
% col = 24;  % ап
% row = ceil( length(In1)/col );
% 
% IndexIn = [1:col*row];
% IndexInReshape = reshape(IndexIn, col,row)';
% intrlvIndex = reshape(IndexInReshape,1,[]);
% 
% Out = zeros(length(intrlvIndex),1);
% for ii = 1:length(intrlvIndex)
%     Out(ii, 1) =  In1( intrlvIndex(ii) );
% end
% Out = Out.';
end

