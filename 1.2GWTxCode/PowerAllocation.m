function [TxPowerOut] = PowerAllocation(TotalPower,Rx_pow,BF,H_estimation,Fre,BW,Re_snr,T_GA,N,PowerDiff1,PowerDiff2,PowerDiff3)
    %% 第1次分配
%   P = (Rx_pow.').*diag((abs(BF*(H_estimation./(sqrt(sum(abs(H_estimation).^2)))))).^2);
    P = Rx_pow.';
    PP = (BW'.*Re_snr'.*T_GA')./P;
    Per = PP./sum(PP);
    
    %% 第2次分配,频谱重叠的用户，每个用户发送功率与其中的最大功率用户相同
    BWOverlop = 1.0;
    TxPower0 = TotalPower.*Per;
    for i = 1:N
        TempTxPower = zeros(1,N);
        TempFlag = 0;
        for j = 1:N
            if i ~= j
                if BWOverlop*(BW(j)+BW(i)) >= abs( 2*(Fre(i)-Fre(j)) )
                    
                else
                    if TxPower0(j) > TxPower0(i)*db2pow(PowerDiff1)
                        TempFlag = 1;
                        TempTxPower(j) = TxPower0(j);
                    end
                end
            end
        end
        if TempFlag==1
%             TxPower0(i) = max(TempTxPower);
%             TxPower0(i) = max(TempTxPower)/15;
            TxPower0(i) = max(TempTxPower)/db2pow(PowerDiff1);
        end
    end
    TxPowerPer = TxPower0./sum(TxPower0);
    TxPower1 = TxPowerPer.*TotalPower;
    %% 第3次分配，最小功率限制、分配功率不低于（最大功率-10dB）或者（总功率-20dB)
    MaxTxPower = max(TxPower1);
    if (MaxTxPower/db2pow(PowerDiff2)) < (TotalPower/(db2pow(PowerDiff3)))
        PowerLimit = TotalPower/(db2pow(PowerDiff3));
%         PowerLimit = MaxTxPower/SpecOverLap;
    else
        PowerLimit = MaxTxPower/db2pow(PowerDiff2);
%         PowerLimit = PowerDiff*TotalPower;
    end
    UserNum = 0;
    NewSumPowerPer = 0;
    TxPower2 = TxPower1;
    for i=1:N
        if TxPower1(i) < (MaxTxPower/db2pow(PowerDiff2)) || TxPower1(i)< (TotalPower/(db2pow(PowerDiff3)))
            TxPowerPer(i) = 0;
            TxPower2(i) = PowerLimit;
            UserNum = UserNum + 1;
        end
%         NewSumPowerPer = NewSumPowerPer + TxPowerPer(i);
    end
%     for i=1:N
%         if TxPowerPer(i)==0
%             
%         else
%             TxPowerPer(i) = TxPowerPer(i)/NewSumPowerPer;
%             TxPower2(i) = (TotalPower - UserNum*PowerLimit)*TxPowerPer(i);
%         end
%     end
    %% 输出
    TxPowerPer = TxPower2./sum(TxPower2);
    TxPowerOut = (TxPowerPer.*TotalPower).';
    
end

