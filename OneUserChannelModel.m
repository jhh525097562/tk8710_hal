function [Rx,TO] = OneUserChannelModel(TerminalTxSignal,SimPara,SampleRate,SnrSamplingRate,PathAttenuation)

Txinput1 = TerminalTxSignal(1,:);
fs = SampleRate;

% SnrSamplingRate = SysSimulationPara.GWRxSNR-10*log10(16/4*64)-pow2db(50);
% SnrSamplingRate = SysSimulationPara.GWRxSNR-10*log10(16/4*64);
noiseVar = 1/10^(SnrSamplingRate/10);
CFO_freq = 0;
var = 0;
%% TimingOffset
if SimPara.TimingOffsetFlag==0%not add TimeOffset
    Tx = [Txinput1.';zeros(floor(fs*0.006),1)];
    TO(1,1) = 0;
elseif SimPara.TimingOffsetFlag==1%add rand TimeOffset
    Tx = [zeros(length(Txinput1.')+floor(fs*0.007-16),1)];
    TimingOffset = zeros(1,randi(floor(fs*SimPara.TimingOffsetSet/1e6)));
    TO(1,1) = length(TimingOffset);
    Tx(1:(length(TimingOffset)+length(Txinput1'))) = [TimingOffset';Txinput1.'];
elseif SimPara.TimingOffsetFlag==2%add point TimeOffset
    Tx = [zeros(length(Txinput1.')+floor(fs*0.007-16),1)];
    TimingOffset = zeros(1,floor(fs*SimPara.TimingOffsetSet/1e6));
    TO(1,1) = length(TimingOffset);
    Tx(1:(length(TimingOffset)+length(Txinput1'))) = [TimingOffset';Txinput1.'];
end
%% CFO\Noise\DopplerFreq Set
DopplerFreq = SimPara.DopplerFreq; %
if DopplerFreq == 0
    Tag  = 'CFO';  % 1: CFO    2:Doppler    3:  CFODoppler
    CFO_freq = SimPara.CFOFreqSet;
    var = noiseVar;
elseif DopplerFreq ~=0
    Tag  =  'CFODoppler';
    %         PathDelays = [0 0.1e-6 0.3e-6 0.5e-6 0.8e-6 1.1e-6 1.3e-6 1.7e-6 2.3e-6 3.1e-6 3.2e-6 5.0e-6 ];  % Path delays (s)
    %         PathPower =  [-4  -3  0  -2.6  -3.0  -5.0  -7.0  -5.0  -6.5  -8.6  -11.0  -10 ];   % Path power (dB)
    PathDelays = [0 0.1e-6 0.15e-6 0.3e-6 0.5e-6];  % Path delays (s)
    PathPower =  [-4  -3  0  -2.6  -3.0];   % Path power (dB)
    % Doppler
%     rayChan = comm.RayleighChannel('SampleRate',fs,'PathDelays',PathDelays,'AveragePathGains',PathPower, 'NormalizePathGains',true, ...
%         'MaximumDopplerShift',DopplerFreq,'FadingTechnique','Sum of sinusoids','PathGainsOutputPort',true);
    rayChan = comm.RayleighChannel('SampleRate',fs,'PathDelays',PathDelays,'AveragePathGains',PathPower, 'NormalizePathGains',true, ...
        'MaximumDopplerShift',DopplerFreq,'FadingTechnique','Sum of sinusoids','RandomStream','mt19937ar with seed','Seed',Iter,'PathGainsOutputPort',true);

    ChanDoppler_rayChan = rayChan;
    CFO_freq = SimPara.CFOFreqSet;
    var = noiseVar;
end
len = length(Tx);
%     %% Impulse Noise  % 1ms
%     if ImpulseNoiseFlag==1
%         Sigma2_I=5*var;
%         ImpulsiveNoise_BG = zeros(len,1);
%         coe_i = randi(len-fs*0.001);
%         coe_j = coe_i + fs*0.001;
%         ImpulsiveNoise_BG(coe_i:coe_j,1) = sqrt(Sigma2_I/2).* (1 + (1j));
%     else
%         ImpulsiveNoise_BG = zeros(len,1);
%     end
%    %plot(abs(ImpulsiveNoise_BG))
%% FreqOffsetDrift
if SimPara.DriftPerSecFlag == 0%no FreqOffsetDrift
    tmpExp = ones(1,len);
else %rand FreqOffsetDrift
    tmpPos1=0:1:len-1;
    %         DriftPerSec = randi(TerminalTxRxPara.DriftPerSecSet(1));
    DriftPerSec = SimPara.DriftPerSecSet;
    ChannelFreqOffsetDriftTabletmp = 0:DriftPerSec/fs:DriftPerSec/fs*(len-1);
    %way1
    tmpPhase = pi.*ChannelFreqOffsetDriftTabletmp.*(1/fs).*tmpPos1;
    %way2
    %         tmpPhasedelta = 2*pi.*ChannelFreqOffsetDriftTabletmp.*(1/fs);
    %         tmp = 0;
    %         for ii = 1:length(tmpPhasedelta)
    %             tmp = tmp + tmpPhasedelta(ii);
    %             tmpPhase(ii) = tmp ;
    %         end
    tmpExp = exp(1j*tmpPhase);
end
%     plot(phase(tmpExp1));  %debug
%% Path Attenuation
Tx = Tx.*sqrt(db2pow(PathAttenuation(1)));
%% CFO or Doppler Fading
switch Tag
    case 'CFO'
        len = length(Tx);
        H = exp((1j)*2*pi*CFO_freq*(1:len)'/fs);
        Noise = (randn(len,1) + (1j)*randn(len,1)) * sqrt(var/2);
        Rx(:,1) = Tx.*H.*tmpExp' + 0*Noise;
        %             plot(phase(tmpExp));  %debug
        PSig = mean(abs(Tx(100 : 400 )).^2);
        PNoise = mean(abs(Noise).^2);
        P_SNR = PSig/PNoise;
        10*log10(P_SNR);
        
        
    case 'Doppler'
        len = length(Tx);
        Noise = (randn(len,1) + (1j)*randn(len,1)) * sqrt(var/2);
        
        while true
            [RxTmp, HTmp] = step(rayChan,Tx);
            H = sum(HTmp,2);
            % plot(abs(H));
            if min(abs(H)) > 0 % ��ȥ��
                Rx(:,1) = Tx.*H.*tmpExp' + 0*Noise;
                break
            end
            %                 if min(abs(H)) < 0.1
            %                     a = 1;
            %                 end
            %                 Rx = Tx.*H.*tmpExp' + Noise + ImpulsiveNoise_BG;
            %                 break
        end
        
    case 'CFODoppler'
        len = length(Tx);
        Noise = (randn(len,1) + (1j)*randn(len,1)) * sqrt(var/2);
        
        while true
            [RxTmp, HTmp] = step(ChanDoppler_rayChan,Tx);
            H1 = sum(HTmp,2);
            if min(abs(H1)) > 0
                
                H = H1.*exp((1j)*2*pi*CFO_freq*(1:len)'/fs).*tmpExp';
                %     plot(phase(H));  %debug
                Rx(:,1) = Tx.*H + 0*Noise;
                %     plot(abs(Rx).^2);  %debug
                break
            end
        end
        
    otherwise
        error('unsupported channel type\n');
end


end