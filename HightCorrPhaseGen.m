clear all;
close all;


AntennaNum = 8;
UserNum = 128;

UserPhase = zeros(AntennaNum,1);
ExpectCorr = [0.99 0.98 0.95 0.9 0.8 0.5 0.2 0.1 0.001];
UserPhase_Corr_0_99 = [];
UserPhase_Corr_0_98 = [];
UserPhase_Corr_0_95 = [];
UserPhase_Corr_0_9 = [];
UserPhase_Corr_0_8 = [];
UserPhase_Corr_0_5 = [];
UserPhase_Corr_0_2 = [];
UserPhase_Corr_0_1 = [];
UserPhase_Corr_0_001 = [];
% CorrDif = 0.005;
CorrDif = 0.0005;
SaveNum = 128;
v1 = exp(2*1i*UserPhase.');
for i=1:10000000
    
    v2 = exp(2*1i*(-500 + 1000*rand(AntennaNum,1))*100);
    out = abs(v1*v2(1:AntennaNum))/norm(v1(1:AntennaNum))/norm(v2(1:AntennaNum));
    if out >=ExpectCorr(1) && length(UserPhase_Corr_0_99) <SaveNum
        UserPhase_Corr_0_99 = [UserPhase_Corr_0_99 v2];
    elseif out >=ExpectCorr(2) && out < ExpectCorr(1) && length(UserPhase_Corr_0_98) <SaveNum
        UserPhase_Corr_0_98 = [UserPhase_Corr_0_98 v2];
    elseif out >=(ExpectCorr(3) - CorrDif) && out <(ExpectCorr(3) + CorrDif) && length(UserPhase_Corr_0_95) <SaveNum
        UserPhase_Corr_0_95 = [UserPhase_Corr_0_95 v2];
    elseif out >=(ExpectCorr(4) - CorrDif) && out <(ExpectCorr(4) + CorrDif) && length(UserPhase_Corr_0_9) <SaveNum
        UserPhase_Corr_0_9 = [UserPhase_Corr_0_9 v2];
    elseif out >=(ExpectCorr(5) - CorrDif) && out <(ExpectCorr(5) + CorrDif) && length(UserPhase_Corr_0_8) <SaveNum
        UserPhase_Corr_0_8 = [UserPhase_Corr_0_8 v2];
    elseif out >=(ExpectCorr(6) - CorrDif) && out <(ExpectCorr(6) + CorrDif) && length(UserPhase_Corr_0_5) <SaveNum
        UserPhase_Corr_0_5 = [UserPhase_Corr_0_5 v2];
    elseif out >=(ExpectCorr(7) - CorrDif) && out <(ExpectCorr(7) + CorrDif) && length(UserPhase_Corr_0_2) <SaveNum
        UserPhase_Corr_0_2 = [UserPhase_Corr_0_2 v2];
    elseif out >=(ExpectCorr(8) - CorrDif) && out <(ExpectCorr(8) + CorrDif) && length(UserPhase_Corr_0_1) <SaveNum
        UserPhase_Corr_0_1 = [UserPhase_Corr_0_1 v2];
    elseif out >=(ExpectCorr(9) - CorrDif) && out <(ExpectCorr(9) + CorrDif) && length(UserPhase_Corr_0_001) <SaveNum
        UserPhase_Corr_0_001 = [UserPhase_Corr_0_001 v2];
    end
end

save UserPhase_Corr_0_99 UserPhase_Corr_0_99;
save UserPhase_Corr_0_98 UserPhase_Corr_0_98;
save UserPhase_Corr_0_95 UserPhase_Corr_0_95;
save UserPhase_Corr_0_9 UserPhase_Corr_0_9;
save UserPhase_Corr_0_8 UserPhase_Corr_0_8;
save UserPhase_Corr_0_5 UserPhase_Corr_0_5;
save UserPhase_Corr_0_2 UserPhase_Corr_0_2;
save UserPhase_Corr_0_1 UserPhase_Corr_0_1;
save UserPhase_Corr_0_001 UserPhase_Corr_0_001;