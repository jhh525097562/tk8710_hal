function [outputArg1] = Pinv_Matlab_C(inputArg1)
%PINV_MATLAB_C Summary of this function goes here
%   Detailed explanation goes here
[U_est,S_est,V_est] = SVD_Single_Jacobi_Algorithm_Complex(inputArg1,8,7);
num = size(S_est,1);
countNoZero = 0;
tol = 0;
for i=1:7
    if 10*log10(S_est(i,i)/S_est(i+1,i+1)) > 20
        tol = S_est(i+1,i+1);
        break;
    end
end
for i=1:num
    if abs(S_est(i,i))>tol
        countNoZero = countNoZero + 1;
    end
end
tempMat = zeros(num,num);
for i=1:countNoZero
    tempMat(i,i) = 1/S_est(i,i);
end
outputArg1 = V_est*tempMat*U_est';
end

