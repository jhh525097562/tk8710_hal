function [AHOut] = SysAHGen(SimPara,GWRXPara,TxUserNum,Iter)

s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);
A = SimPara.AntennaNum;%天线配置
M = GWRXPara.AntennaNum;
K = dec2bin(GWRXPara.AntennaWorkStatus,8);
if SimPara.AHFlag==0
    %     if SysSimulationPara.SingleToneInterferenceFlag==0%无单频干扰
    %         if SysSimulationPara.Periodic_InterferenceFlag==0%无周期干扰
    %             TxUserNum1 = TxUserNum;
    %         else
    %             TxUserNum1 = TxUserNum+1;
    %         end
    %
    %     else%有单频干扰
    %         if SysSimulationPara.Periodic_InterferenceFlag==0%无周期干扰
    %             TxUserNum1 = TxUserNum+1;
    %         else
    %             TxUserNum1 = TxUserNum+2;
    %         end
    %     end
        AHOut = (randn(M,TxUserNum) + 1i* randn(M,TxUserNum));
    for i=1:M
        if K(M-i+1)==dec2bin(1)

        else
            AHOut(i,:) = 0;
        end
    end
    for i = 1:TxUserNum
        AHOut(:,i) = sqrt(A)*AHOut(:,i)/sqrt(sum(abs(AHOut(:,i).^2))); %
    end
else
    if SimPara.AHFlag==0.99
        load('UserPhase_Corr_0_99.mat');
        UserAH = UserPhase_Corr_0_99;
    elseif SimPara.AHFlag==0.98
        load('UserPhase_Corr_0_98.mat');
        UserAH = UserPhase_Corr_0_98;
    elseif SimPara.AHFlag==0.95
        load('UserPhase_Corr_0_95.mat');
        UserAH = UserPhase_Corr_0_95;
    elseif SimPara.AHFlag==0.9
        load('UserPhase_Corr_0_9.mat');
        UserAH = UserPhase_Corr_0_9;
    elseif SimPara.AHFlag==0.8
        load('UserPhase_Corr_0_8.mat');
        UserAH = UserPhase_Corr_0_8;
    elseif SimPara.AHFlag==0.5
        load('UserPhase_Corr_0_5.mat');
        UserAH = UserPhase_Corr_0_5;
    elseif SimPara.AHFlag==0.2
        load('UserPhase_Corr_0_2.mat');
        UserAH = UserPhase_Corr_0_2;
    elseif SimPara.AHFlag==0.1
        load('UserPhase_Corr_0_1.mat');
        UserAH = UserPhase_Corr_0_1;
    elseif SimPara.AHFlag==0.001
        load('UserPhase_Corr_0_001.mat');
        UserAH = UserPhase_Corr_0_001;
    else
        
    end
    UserAH(:,1) = ones(1,M).';
    for i = 1:TxUserNum
        AHOut(:,i) = sqrt(M)*UserAH(:,i)/sqrt(sum(abs(UserAH(:,i).^2))); %
    end

end

end
