function [OutSignal] = SigmaDelta(InputSignal)

MaxAmplitude = max(abs(InputSignal));
InputSignal = InputSignal./MaxAmplitude;
N = length(InputSignal);
OutSignal = zeros(1,N);

%% Way2
integrator = 0;
for i=1:N
    if integrator >=0
        quantized = 1;
    else
        quantized = -1;
    end
    OutSignal(i) = quantized;
    integrator = integrator + InputSignal(i) - quantized;
end
end

