function signalArr = compareBest(signalReceive, filterLen, antennaNum, Hhat)

% 2^7种消除组合

[numCh, signalCol] = size(signalReceive);

% 滑动次数
slideTime = signalCol/(filterLen*antennaNum);

signalArr = zeros(numCh*2^(antennaNum-1),filterLen*slideTime);
% 处理每个用户
for ii = 1:numCh
    % 处理每次滑动
    for jj = 1:slideTime
        % 截取部分数据
        sigSeg = transpose(reshape(signalReceive(ii,(jj-1)*filterLen*antennaNum+1:jj*filterLen*antennaNum),filterLen,antennaNum));
        
        % 求相关
        HNormal = Hhat(:,ii)/sqrt(sum(abs(Hhat(:,ii).^2)));
        inphaseComponent = HNormal'*sigSeg;
        orthogonalComponent = sigSeg - HNormal*inphaseComponent;
        R = orthogonalComponent*orthogonalComponent';
        [U,G,V] = svd(R);

        % 抑制分量
        SDDecoder = zeros(2^(antennaNum-1),filterLen);
        for mm = 1:2^(antennaNum-1)
            indexLag = dec2bin(mm-1) == '1';
            index = find(indexLag);
            len = length(index);
            inp = inphaseComponent;
            for ll = 1:len
                S = V(:,index(ll))'*orthogonalComponent; 
                A = inp*S'/(S*S');
                inp = inp - A*S;
            end
            SDDecoder(mm,:) = inp;
        end
            
        signalArr((ii-1)*2^(antennaNum-1)+1:ii*2^(antennaNum-1),(jj-1)*filterLen+1:jj*filterLen) = SDDecoder;
    end
end

end