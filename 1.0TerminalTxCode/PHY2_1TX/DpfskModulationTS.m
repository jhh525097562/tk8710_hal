function  sig  = DpfskModulationTS(Mfsk,intrlvBits1,intrlvBits2,FskPreamble,PskPreamblePhase,SymNumPreamble,SymNumData,LenSymPhase,Mod4Psk,FskPhase,TransRatio)
% global LenSymPhase;
FlagQpi = 0;

LenSym1 = LenSymPhase;
rollLen = LenSym1*TransRatio ;% 1/2 1/4 1/8 符号周期

HalfWinLen = rollLen/2;
halfWin = [1/2+1/2*cos(pi*(0:rollLen)/rollLen)];
WinR = halfWin(1,:);
WinL = fliplr(halfWin(1,:));

% figure;plot(WinR);
% figure;plot(WinL);

sigPre = zeros(LenSym1*SymNumPreamble,1);
sigPreArr = zeros(SymNumPreamble,LenSym1);
sigPreArrF = zeros(SymNumPreamble,LenSym1);

for iSym=1:SymNumPreamble
    sigPreArr(iSym,:) = (FskPhase(:,FskPreamble(iSym)) + PskPreamblePhase(iSym));
end

sigPreArrPhase = sigPreArr(1,end);
for iSym=1:SymNumPreamble-1
    while(1)
        phaseTmp = sigPreArr(iSym+1,1) - sigPreArrPhase;
        if phaseTmp > pi
            sigPreArr(iSym+1,:) = sigPreArr(iSym+1,:) - 2*pi;
        else if phaseTmp < -pi
                sigPreArr(iSym+1,:) = sigPreArr(iSym+1,:) + 2*pi;
            else if phaseTmp < pi && phaseTmp > -pi
                    sigPreArrPhase = sigPreArr(iSym+1,end);
                    break;
                end
            end
        end
    end
end

for iSym=1:SymNumPreamble
    sigPreArrF(iSym,:) = sigPreArr(iSym,:);
end

Ri = HalfWinLen*2;
Li = HalfWinLen;
Midi = LenSym1 - HalfWinLen;

for iSym=1:SymNumPreamble-1
    sigPreArrR = sigPreArr(iSym,1:Ri+1) + sigPreArr(iSym,Midi+1) - sigPreArr(iSym,1); % 前一个符号尾部16个点
    sigPreArrL = sigPreArr(iSym+1,1:Ri+1) + sigPreArr(iSym+1,Li+1) - sigPreArr(iSym+1,Ri+1); % 后一个符号头部17个点
    sigPreArrF(iSym,LenSym1 - HalfWinLen+1:LenSym1) = sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);
    sigPreArrF(iSym+1,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);
end

sigPreArrPhase = sigPreArrF(SymNumPreamble,end);
sigPre = reshape(sigPreArrF.',1,[]);
% sigPre = reshape(sigPreArr.',1,[]);

switch Mfsk
    case 2
        %2fsk
        bits1 = intrlvBits1(1:end);
        %dqpsk
        bits3 = intrlvBits2(1:2:end);
        bits4 = intrlvBits2(2:2:end);
        
        Reference = PskPreamblePhase(end); % 最后一个preamble的psk是第一个数据dqpsk的参考
        sigDPFSK = zeros((SymNumData+SymNumPreamble)*LenSym1,1);  % 分配充分的信号长度
        sigDPFSK(1:length(sigPre))=sigPre;  % preamble部分
        
        sigDPFSKtempArr = zeros(SymNumData,LenSym1);
        sigDPFSKtempArrF = zeros(SymNumData,LenSym1);
        
        for iSym=1:SymNumData
            tmp1 =  bits1(iSym) + 1; %
            sigDPFSKtempArr( iSym,1 : LenSym1 )= FskPhase(:,tmp1);
            tmp2phase = Reference + PhaseMod4Psk([bits3(iSym);bits4(iSym)]);
%             tmp2= Reference * step(Mod4Psk, [bits3(iSym);bits4(iSym)]); % dqpsk
%             tmp2phase = phase(tmp2);
            sigDPFSKtempArr( iSym,1 : LenSym1 ) = (sigDPFSKtempArr( iSym,1 : LenSym1 ) + tmp2phase);
            Reference = tmp2phase; % 更新参考
        end
        
        for iSym=0:SymNumData-1
            while(1)
                phaseTmp = sigDPFSKtempArr(iSym+1,1) - sigPreArrPhase;%sigPreArrPhase  sigPreArr(iSym,LenSym1)
                if phaseTmp > pi
                    sigDPFSKtempArr(iSym+1,:) = sigDPFSKtempArr(iSym+1,:) - 2*pi;
                elseif phaseTmp < -pi
                    sigDPFSKtempArr(iSym+1,:) = sigDPFSKtempArr(iSym+1,:) + 2*pi;
                elseif phaseTmp < pi && phaseTmp > -pi
                    sigPreArrPhase = sigDPFSKtempArr(iSym+1,LenSym1);
                    break;
                end
            end
        end
        
        sigDPFSKtempArrF = sigDPFSKtempArr;
        for iSym=1:SymNumData-1
            if iSym == 1
                sigPreArrR = sigPreArr(SymNumPreamble,1:Ri+1) + sigPreArr(SymNumPreamble,Midi+1) - sigPreArr(SymNumPreamble,1);
                sigPreArrL = sigDPFSKtempArr(iSym,1:Ri+1) + sigDPFSKtempArr(iSym,Li+1) - sigDPFSKtempArr(iSym,Ri+1);
                sigDPFSKtempArrF(iSym,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);
                sigDPFSK((SymNumPreamble-1)*LenSym1 + Midi+1:SymNumPreamble*LenSym1) =  sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);
            end
            sigPreArrR = sigDPFSKtempArr(iSym,1:Ri+1) + sigDPFSKtempArr(iSym,Midi+1) - sigDPFSKtempArr(iSym,1);
            sigPreArrL = sigDPFSKtempArr(iSym+1,1:Ri+1) + sigDPFSKtempArr(iSym+1,Li+1) - sigDPFSKtempArr(iSym+1,Ri+1);
            sigDPFSKtempArrF(iSym,LenSym1 - HalfWinLen+1:LenSym1) = sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);
            sigDPFSKtempArrF(iSym+1,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);
        end
        
        sigDPFSK(length(sigPre)+1:end) = reshape(sigDPFSKtempArrF.',1,[]);
        sig = sigDPFSK;
        
    case 4
        %4fsk
        bits1 = intrlvBits1(1:2:end);
        bits2 = intrlvBits1(2:2:end);
        
        %dqpsk
        bits3 = intrlvBits2(1:2:end);
        bits4 = intrlvBits2(2:2:end);
        
        Reference = PskPreamblePhase(end); % 最后一个preamble的psk是第一个数据dqpsk的参考
        sigDPFSK = zeros((SymNumData+SymNumPreamble)*LenSym1,1);  % 分配充分的信号长度
        sigDPFSK(1:length(sigPre))=sigPre;  % preamble部分
        
        sigDPFSKtempArr = zeros(SymNumData,LenSym1);
        sigDPFSKtempArrF = zeros(SymNumData,LenSym1);
        
        for iSym=1:SymNumData
            tmp1 =  Graymapping([],[],bits1(iSym),bits2(iSym),Mfsk); % graymapping 00 1  01 2  11 3   10 4
            sigDPFSKtempArr( iSym,1 : LenSym1 )= FskPhase(:,tmp1);
            tmp2phase = Reference + PhaseMod4Psk([bits3(iSym);bits4(iSym)]);
%             tmp2 = Reference*step(Mod4Psk,[bits3(iSym);bits4(iSym)]); % dqpsk
%             tmp2phase = phase(tmp2);
            sigDPFSKtempArr( iSym,1 : LenSym1 ) = (sigDPFSKtempArr( iSym,1 : LenSym1 ) + tmp2phase);
            Reference = tmp2phase; % 更新参考
        end
        
        for iSym=0:SymNumData-1
            while(1)
                phaseTmp = sigDPFSKtempArr(iSym+1,1) - sigPreArrPhase;%sigPreArrPhase  sigPreArr(iSym,LenSym1)
                if phaseTmp > pi
                    sigDPFSKtempArr(iSym+1,:) = sigDPFSKtempArr(iSym+1,:) - 2*pi;
                else if phaseTmp < -pi
                        sigDPFSKtempArr(iSym+1,:) = sigDPFSKtempArr(iSym+1,:) + 2*pi;
                    else if phaseTmp < pi && phaseTmp > -pi
                            sigPreArrPhase = sigDPFSKtempArr(iSym+1,LenSym1);
                            break;
                        end
                    end
                end
            end
        end
        
        sigDPFSKtempArrF = sigDPFSKtempArr;
        for iSym=1:SymNumData-1
            if iSym == 1
                sigPreArrR = sigPreArr(SymNumPreamble,1:Ri+1) + sigPreArr(SymNumPreamble,Midi+1) - sigPreArr(SymNumPreamble,1);
                sigPreArrL = sigDPFSKtempArr(iSym,1:Ri+1) + sigDPFSKtempArr(iSym,Li+1) - sigDPFSKtempArr(iSym,Ri+1);
                sigDPFSKtempArrF(iSym,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);
                sigDPFSK((SymNumPreamble-1)*LenSym1 + Midi+1:SymNumPreamble*LenSym1) =  sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);
            end
            sigPreArrR = sigDPFSKtempArr(iSym,1:Ri+1) + sigDPFSKtempArr(iSym,Midi+1) - sigDPFSKtempArr(iSym,1);
            sigPreArrL = sigDPFSKtempArr(iSym+1,1:Ri+1) + sigDPFSKtempArr(iSym+1,Li+1) - sigDPFSKtempArr(iSym+1,Ri+1);
            sigDPFSKtempArrF(iSym,LenSym1 - HalfWinLen+1:LenSym1) = sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);
            sigDPFSKtempArrF(iSym+1,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);
        end
        
        sigDPFSK(length(sigPre)+1:end) = reshape(sigDPFSKtempArrF.',1,[]);
        sig = sigDPFSK;
        
        %         figure;
        %         subplot(2,1,1);plot(phase(sigDPFSK));title('phase');
        %         subplot(2,1,2);plot(abs(sigDPFSK));title('abs');
        
end

end