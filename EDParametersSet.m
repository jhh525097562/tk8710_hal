function ED_Parameters = EDParametersSet(SimConfig,N,SysPara)

global Iter;

s = RandStream('mt19937ar','Seed',Iter);
RandStream.setGlobalStream(s);

ED_Parameters = [];

for i = 1:N
    % TxMode
    ED_Parameters(i).TxMode = SimConfig(i,1);

    if ED_Parameters(i).TxMode == SysPara.GWRxMode(1) || ED_Parameters(i).TxMode == SysPara.GWRxMode(5)
        FreBand = SysPara.GWRxModeBW(1);
    elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(2) || ED_Parameters(i).TxMode == SysPara.GWRxMode(6)
        FreBand = SysPara.GWRxModeBW(2);
    elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(3) || ED_Parameters(i).TxMode == SysPara.GWRxMode(7)
        FreBand = SysPara.GWRxModeBW(3);   
    elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(4) || ED_Parameters(i).TxMode == SysPara.GWRxMode(8)
        FreBand = SysPara.GWRxModeBW(4);
    end
    %TxFreq
    if SimConfig(i,2) ==1
        ED_Parameters(i).TxFreq = SimConfig(i,3);
    elseif SimConfig(i,2) ==2
%           ED_Parameters(i).TxFreq = randi([-1024+65,-1024+65+1048],1)*4/8192*125*1e3;
%         if ED_Parameters(i).TxMode == SysPara.GWRxMode(1) || ED_Parameters(i).TxMode == SysPara.GWRxMode(5)
%             FreStart = SysPara.Freqstart(1);
%             FreEnd =  SysPara.FreqEnd(1);
%         elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(2) || ED_Parameters(i).TxMode == SysPara.GWRxMode(6)
%             FreStart = SysPara.Freqstart(2);
%             FreEnd =  SysPara.FreqEnd(2);
%         elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(3) || ED_Parameters(i).TxMode == SysPara.GWRxMode(7)
%             FreStart = SysPara.Freqstart(3);
%             FreEnd =  SysPara.FreqEnd(3);
%         elseif ED_Parameters(i).TxMode == SysPara.GWRxMode(4) || ED_Parameters(i).TxMode == SysPara.GWRxMode(8)
%             FreStart = SysPara.Freqstart(4);
%             FreEnd =  SysPara.FreqEnd(4);         
%         end

          FreStart = SysPara.Freqstart(1);
          FreEnd =  SysPara.FreqEnd(1);    
          ED_Parameters(i).TxFreq = randi([FreStart+FreBand/2,FreEnd-FreBand/2],1);
    end
    
    %RxMode
    ED_Parameters(i).RxMode = SimConfig(i,4); % RF_RxFreq
    
    %RxFreq
    ED_Parameters(i).RxFreq = SimConfig(i,5); % RF_RxFreq
    
    %PolarCCflag
    ED_Parameters(i).PolarCCflag = SimConfig(i,6); % RF_RxFreq
    
    % Range
    if SimConfig(i,7) ==1
        ED_Parameters(i).Range = SimConfig(i,8);
    elseif SimConfig(i,7) ==2
        ED_Parameters(i).Range = randi([300,400],1);
    end
    
    % Angle
    if SimConfig(i,9) ==1
        ED_Parameters(i).Angle = SimConfig(i,10);
    elseif SimConfig(i,9) ==2
        ED_Parameters(i).Angle = randi([1,360],1);
    end
    
end


end