function [output, buffer, result_tmpMiu] = dvbRateConvert(input, in_rate, out_rate , mode , alpha , buffer)
% function output = dvbRateConvert(input, in_rate, out_rate , mode , alpha) attempt to
% sample rate conversion.
%
% Theory desription:
%  y(K*Ti) = sum(x(m*Ts)*h(K*Ti - m*Ts))    sum for m
%  Ti is output sampling period
%  Ts is input sampling period
% Ideal h(t) = sin(t)/t, but it is impossible to pratice.
% we need choose a h(t) by check its frequency response, if it isn't affect
% useful signals frequency.
%
% mode == 0 : use cubic polynomial function to replace h(t)
% C(-2) = 1/6*u^3          - 1/6 * u
% C(-1) = -1/2u^3 + 1/2u^2 - 1     u
% C(0)  = 1/2 u^3 -    u^2 - 1/2   u  +  1
% C(1)  = -1/6u^3 + 1/2u^2 - 1/3   u
% mode == 1 : use piecewise - parabolic interpolator
% C(-2) =  alpha u^2    - alpha u
% C(-1) = -alpha u^2 + (alpha+1)u
% C(0)  = -alpha u^2 + (alpha-1)u  + 1
% C(1)  =  alpha u^2    - alpha u
%
% if alpha = 0, it reduces to linear interpolator.
%
% Notice: because it is specifical for dvb ,so in_rate must be larger than
% out_rate, or performance is bad.


%  Written by Raymond Li, Alan Niu
%  Copyright 2006-2010 The AMLogic, Inc.
%  $Revision: 1.0 $  $Date: 2006/06/15  $
result_tmpMiu = [];
lcm_rate = in_rate * out_rate; %least common multiple of in_rate and out_rate
interpolate_interval = out_rate;
miu = 1 / out_rate ;
decimate_interval = in_rate;
inputData = input;
k = 1;                  % output number counter
output = zeros(1,ceil(length(inputData)*out_rate/in_rate));
window = buffer.window;
counter = buffer.counter;
len_wind = length(window);
for i = 1 : length(inputData)
    window = [window(2:len_wind) inputData(i)];
    % check counter
    if (counter > interpolate_interval)
        counter = counter - interpolate_interval;
    else
        tmpMiu = counter * miu;
        if mode == 0
            %cubic Interpolator
            c11 = (tmpMiu^3)/6 - tmpMiu/6;
            c22 = (-tmpMiu^3)/2 + (tmpMiu^2)/2 + tmpMiu;
            c33 = (tmpMiu^3)/2 - tmpMiu^2 - tmpMiu/2 + 1;
            c44 = (-tmpMiu^3)/6 + (tmpMiu^2)/2 - tmpMiu/3;
            coef = [c44 c33 c22 c11];
        else
            %piecewise-Parabolic Interpolator
            c1 = alpha * tmpMiu * tmpMiu - alpha * tmpMiu;
            c2 = -alpha * tmpMiu * tmpMiu + (alpha + 1) * tmpMiu;
            c3 = -alpha * tmpMiu * tmpMiu + (alpha - 1) * tmpMiu + 1;
            c4 = alpha * tmpMiu * tmpMiu - alpha * tmpMiu;
            coef = [c4 c3 c2 c1];
        end
        output(k) = window * coef';
        k = k + 1;

        counter = counter + decimate_interval - interpolate_interval;

        while(counter < 0)
            tmpMiu = (counter + interpolate_interval) * miu;

            if mode == 0
                %cubic Interpolator
                c11 = (tmpMiu^3)/6 - tmpMiu/6;
                c22 = (-tmpMiu^3)/2 + (tmpMiu^2)/2 + tmpMiu;
                c33 = (tmpMiu^3)/2 - tmpMiu^2 - tmpMiu/2 + 1;
                c44 = (-tmpMiu^3)/6 + (tmpMiu^2)/2 - tmpMiu/3;
                coef = [c44 c33 c22 c11];
            else
                %piecewise-Parabolic Interpolator
                c1 = alpha * tmpMiu * tmpMiu - alpha * tmpMiu;
                c2 = -alpha * tmpMiu * tmpMiu + (alpha + 1) * tmpMiu;
                c3 = -alpha * tmpMiu * tmpMiu + (alpha - 1) * tmpMiu + 1;
                c4 = alpha * tmpMiu * tmpMiu - alpha * tmpMiu;
                coef = [c4 c3 c2 c1];
            end
            output(k) = window * coef';
            k = k + 1;
            counter = counter + decimate_interval;

        end
    end
end

buffer.window = window;
buffer.counter = counter;
output(k:end) = [];

return;

%%

