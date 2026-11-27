function [ signalArr ] = channelize20180408B(signalReceive, Hhat, filterLen, antennaNum, supressNum )
%   function：对多天线频域信号进行合并

%   signalReceive：信号的频域数据，每一行是一路信号，每filterLen列是一个天线数据，每filterLen*antennaNum列是一次滑动数据
%   filterLen：一根天线一路信号一次滑动输出的频域数据
%   antennaNum：天线的数量
%   Hhat：信道估计

%   signalArr：输出信号，numCh*(supressNum+1)行，filterLen*slideTime列

[numCh,signalCol] = size(signalReceive);
% 求滑动次数，由于函数A已经处理了长度，所以这里slideTime为整数
slideTime = signalCol/(filterLen*antennaNum);

signalArr = zeros(numCh*(supressNum+1),filterLen*slideTime);
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
        SDDecoder = zeros(supressNum+1,filterLen);
        SDDecoder(1,:) = inphaseComponent;
        for kk = 1:supressNum
            S = V(:,kk)'*orthogonalComponent; 
            A = inphaseComponent*S'/(S*S');
            inphaseComponent = inphaseComponent - A*S;
            SDDecoder(1+kk,:) = inphaseComponent;
        end
            
        signalArr((ii-1)*(supressNum+1)+1:ii*(supressNum+1),(jj-1)*filterLen+1:jj*filterLen) = SDDecoder;
    end
end



