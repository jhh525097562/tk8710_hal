function [The] = getThreadValue(arr,GWRXPara)

eps = 0.0000001;

maxval = max(arr);
minval = min(arr);

if minval == 0
    minval = eps;
end


midval = sqrt(maxval*minval);

m = size(arr,2);
n_min = round(m*GWRXPara.GWRX_P1_Para1);
n_max = round(m*GWRXPara.GWRX_P1_Para2);

% while 1
for i=1:7%10
    greater = sum(arr > midval);
    less    = sum(arr < midval);
    rate = (maxval / minval) < (GWRXPara.GWRX_ANoiseThe2/GWRXPara.GWRX_ANoiseThe1);
    
    The = midval;
    if (greater >= n_min) && (greater <= n_max) || rate
%         The = midval;
        break
    end
    
    if greater > less
        minval = midval;
    else
        maxval = midval;
    end
    midval = sqrt(maxval*minval);
end
