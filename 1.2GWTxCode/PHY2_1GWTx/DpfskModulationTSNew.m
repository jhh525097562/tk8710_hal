function  sig  = DpfskModulationTSNew(Mfsk,intrlvBits1,intrlvBits2,FskPreamble,PskPreamblePhase,SymNumPreamble,SymNumData,LenSymPhase,FskPhase,TransRatio,FirstTransiPreamble,iBlock,DSeq,DSSSNum)
key = 2;
 pi = 3.1415926535897931;
if key == 0
    LenSym1 = LenSymPhase;
    rollLen = LenSym1*TransRatio ;%  ²ÎÓë¹ý¶ÉµÄ²ÉÑùµã¸öÊýTransRatio =  1/4  1/2  3/4 1 

    HalfWinLen = rollLen/2 -1;
    halfWin =  [1/2+1/2*cos((0:rollLen)*pi/rollLen)];%[1/2+1/2*cos((0:rollLen)*t_tmp)];%
    % WinR = halfWin(1,:);
    % WinL = fliplr(halfWin(1,:));
     WinR = halfWin(1,2:end - 1);%t_halfWin_tmp;%
     WinL = fliplr(WinR);

    Ri = HalfWinLen*2;%Á½±¶´°³¤¶È
    Li = HalfWinLen;%µ¥±¶´°³¤¶È
    Midi = LenSym1 - HalfWinLen;%·ûºÅ³¤¶È¼õÈ¥´°³¤¶È

    % figure;plot(WinR);
    % figure;plot(WinL);

    sigPreArr = zeros(SymNumPreamble,LenSym1);
    for iSym=1:SymNumPreamble
        sigPreArr(iSym,:) = (FskPhase(:,FskPreamble(iSym)) + PskPreamblePhase(iSym));%t_sigDPFSKtempArr;%
    end


           %Êý¾Ýµ÷ÖÆ 
            %4fsk
            bits1 = intrlvBits1(1:2:end);
            bits2 = intrlvBits1(2:2:end);

            %dqpsk
            bits3 = intrlvBits2(1:2:end);
            bits4 = intrlvBits2(2:2:end);

            Reference = PskPreamblePhase(end); % ×îºóÒ»¸öpreamble·ûºÅµÄÏàÎ»
            sigDPFSKtempArr = zeros(SymNumData+SymNumPreamble,LenSym1);
            sigDPFSKtempArr(1:SymNumPreamble,:) = sigPreArr;

            for iSym = 1 : SymNumData
                tmp1 =  Graymapping([],[],bits1(iSym),bits2(iSym),Mfsk); % graymapping 00 1  01 2  11 3   10 4
                tmp2phase = Reference + PhaseMod4Psk([bits3(iSym);bits4(iSym)]);%t_tmp2phase;%
                sigDPFSKtempArr(iSym + SymNumPreamble , 1 : LenSym1 )= FskPhase(:,tmp1) + tmp2phase ;%t_sigDPFSKtempArr_tmp;%
                Reference = tmp2phase; % 
            end

            % DSSS 
            DSeqPhase = phase(DSeq);
            sigDPFSKtempArrDsss = zeros( (SymNumData+SymNumPreamble) * DSSSNum + 1, LenSym1);
            for i = 1:SymNumData+SymNumPreamble
                sigDPFSKtempArrDsss((i-1)*DSSSNum+1:i*DSSSNum,1:LenSym1) = sigDPFSKtempArr(i,1:LenSym1)+DSeqPhase.';%t_sigDPFSKtempArrDsss_tmp;%
            end

            % 
            sigDPFSKtempArrDsss((SymNumData+SymNumPreamble)*DSSSNum+1,:) = sigPreArr(1,:);
            if iBlock > 1  % °ü¿éÊý´óÓÚ2²Å´¦Àí
                sigDPFSKtempArrDsss(1,:) = FirstTransiPreamble.';
            end

           
            % Transition 
            sigDPFSKtempArrTS = zeros( (SymNumData+SymNumPreamble) * DSSSNum + 1, LenSym1);
            calculate = zeros((SymNumData+SymNumPreamble) * DSSSNum +1 , LenSym1);
            sigDPFSKtempArrTS(1,1:LenSym1-HalfWinLen ) = sigDPFSKtempArrDsss(1,1: LenSym1-HalfWinLen );
            for iSym= 1 : (SymNumData+SymNumPreamble)*DSSSNum  
                while(1)
                    phaseTmp = sigDPFSKtempArrDsss(iSym+1,1) - sigDPFSKtempArrDsss(iSym,LenSym1);%sigPreArrPhase  sigPreArr(iSym,LenSym1)
                    if phaseTmp > pi
                        sigDPFSKtempArrDsss(iSym+1,:) = sigDPFSKtempArrDsss(iSym+1,:) - 2*pi;%t_sigDPFSKtempArrDsss_tmp2;%
                      else if phaseTmp < -pi
                            sigDPFSKtempArrDsss(iSym+1,:) = sigDPFSKtempArrDsss(iSym+1,:) + 2*pi;%t_sigDPFSKtempArrDsss_tmp2;%
                        else if phaseTmp < pi && phaseTmp > -pi
                                break;
                            end
                        end
                    end
                end

    %             sigPreArrR = sigDPFSKtempArrDsss(iSym,1:Ri+1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,1);
                sigPreArrR = sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri);%°ÑÇ°Ò»¸ö·ûºÅÎ²²¿ÍùºóÑÓ³¤Li+1¸öµãÈ»ºóÈ¡ÑÓ³¤ºóµÄÎ²²¿Ri+1¸öµã
                sigPreArrL = sigDPFSKtempArrDsss(iSym+1,1:Ri+1) + sigDPFSKtempArrDsss(iSym+1,Li+1) - sigDPFSKtempArrDsss(iSym+1,Ri+1);%°ÑºóÒ»¸ö·ûºÅÍùÇ°ÑÓ³¤Li+1¸öµãÈ»ºóÈ¡ÑÓ³¤ºóµÄÍ·²¿Ri+1¸öµã
                
                calculate(iSym,LenSym1-HalfWinLen+1:LenSym1) = calculate(iSym,LenSym1-HalfWinLen+1:LenSym1) + sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li) - sigDPFSKtempArrDsss(iSym,LenSym1-HalfWinLen+1:LenSym1);
                calculate(iSym +1 ,1:HalfWinLen) =  calculate(iSym +1 ,1:HalfWinLen) + sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li) - sigDPFSKtempArrDsss(iSym +1 ,1:HalfWinLen);
                
%                 sigDPFSKtempArrTS(iSym,LenSym1-HalfWinLen+1:LenSym1) = sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);%t_sigDPFSKtempArrTS_tmp;%
%                 sigDPFSKtempArrTS(iSym+1,1:HalfWinLen) = sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);%t_sigDPFSKtempArrTS_tmp;%

%                 sigDPFSKtempArrTS(iSym+1,HalfWinLen + 1: LenSym1) = sigDPFSKtempArrDsss(iSym+1,HalfWinLen + 1: LenSym1);
            end
            sigDPFSKtempArrTS = sigDPFSKtempArrDsss + calculate;
            sig = reshape(sigDPFSKtempArrTS.',1,[]).'; 

            %  figure;
            %  subplot(2,1,1);plot(phase(sigDPFSK));title('phase');
            %  subplot(2,1,2);plot(abs(sigDPFSK));title('abs');

end

if key == 1
    LenSym1 = LenSymPhase;
    rollLen = LenSym1*TransRatio ;%  ²ÎÓë¹ý¶ÉµÄ²ÉÑùµã¸öÊýTransRatio =  1/4  1/2  3/4 1 

    HalfWinLen = rollLen/2 -1;
    t_tmp = pi/rollLen;% on and off
    t_tmp = round(t_tmp*2^14)/2^14;
    halfWin =  [1/2+1/2*cos((0:rollLen)*t_tmp)];%
    % WinR = halfWin(1,:);
    % WinL = fliplr(halfWin(1,:));
    t_halfWin_tmp = round(halfWin(1,2:end - 1).*2^15)./2^15;%on and of
     WinR = t_halfWin_tmp;%halfWin(1,2:end - 1);%
     WinL = fliplr(WinR);

    Ri = HalfWinLen*2;%Á½±¶´°³¤¶È
    Li = HalfWinLen;%µ¥±¶´°³¤¶È
    Midi = LenSym1 - HalfWinLen;%·ûºÅ³¤¶È¼õÈ¥´°³¤¶È

    % figure;plot(WinR);
    % figure;plot(WinL);

    sigPreArr = zeros(SymNumPreamble,LenSym1);
    for iSym=1:SymNumPreamble
        t_sigDPFSKtempArr = round((FskPhase(:,FskPreamble(iSym)) + PskPreamblePhase(iSym)).*2^7)./2^7;%on and off
        sigPreArr(iSym,:) = t_sigDPFSKtempArr;%(FskPhase(:,FskPreamble(iSym)) + PskPreamblePhase(iSym));%
    end


           %Êý¾Ýµ÷ÖÆ 
            %4fsk
            bits1 = intrlvBits1(1:2:end);
            bits2 = intrlvBits1(2:2:end);

            %dqpsk
            bits3 = intrlvBits2(1:2:end);
            bits4 = intrlvBits2(2:2:end);

            Reference = PskPreamblePhase(end); % ×îºóÒ»¸öpreamble·ûºÅµÄÏàÎ»
            sigDPFSKtempArr = zeros(SymNumData+SymNumPreamble,LenSym1);
            sigDPFSKtempArr(1:SymNumPreamble,:) = sigPreArr;

            for iSym = 1 : SymNumData
                tmp1 =  Graymapping([],[],bits1(iSym),bits2(iSym),Mfsk); % graymapping 00 1  01 2  11 3   10 4
                t_tmp2phase = round((Reference + PhaseMod4Psk([bits3(iSym);bits4(iSym)])).*2^7)./2^7;
                tmp2phase = t_tmp2phase;%(Reference + PhaseMod4Psk([bits3(iSym);bits4(iSym)]));%
                t_sigDPFSKtempArr_tmp = round((FskPhase(:,tmp1) + tmp2phase).*2^7)./2^7;%on and off
                sigDPFSKtempArr(iSym + SymNumPreamble , 1 : LenSym1 )= t_sigDPFSKtempArr_tmp;%(FskPhase(:,tmp1) + tmp2phase);%
                Reference = tmp2phase; % 
            end

            % DSSS 
            DSeqPhase = phase(DSeq);
            sigDPFSKtempArrDsss = zeros( (SymNumData+SymNumPreamble) * DSSSNum + 1, LenSym1);
            for i = 1:SymNumData+SymNumPreamble
                t_sigDPFSKtempArrDsss_tmp = round((sigDPFSKtempArr(i,1:LenSym1)+DSeqPhase.').*2^7)./2^7;%on and off
                sigDPFSKtempArrDsss((i-1)*DSSSNum+1:i*DSSSNum,1:LenSym1) = t_sigDPFSKtempArrDsss_tmp;%(sigDPFSKtempArr(i,1:LenSym1)+DSeqPhase.');
            end

            % 
            sigDPFSKtempArrDsss((SymNumData+SymNumPreamble)*DSSSNum+1,:) = sigPreArr(1,:);
            if iBlock > 1  % °ü¿éÊý´óÓÚ2²Å´¦Àí
                sigDPFSKtempArrDsss(1,:) = FirstTransiPreamble.';
            end

            % Transition 
            sigDPFSKtempArrTS = zeros( (SymNumData+SymNumPreamble) * DSSSNum, LenSym1);
            sigDPFSKtempArrTS(1,1:LenSym1-HalfWinLen ) = sigDPFSKtempArrDsss(1,1: LenSym1-HalfWinLen );
            for iSym= 1 : (SymNumData+SymNumPreamble)*DSSSNum 
                while(1)
                    t_phaseTmp = round((sigDPFSKtempArrDsss(iSym+1,1) - sigDPFSKtempArrDsss(iSym,LenSym1)).*2^4)./2^4;
                    phaseTmp = t_phaseTmp;%sigDPFSKtempArrDsss(iSym+1,1) - sigDPFSKtempArrDsss(iSym,LenSym1);
                    if phaseTmp > pi
                        t_sigDPFSKtempArrDsss_tmp2 = round((sigDPFSKtempArrDsss(iSym+1,:) - 2*pi).*2^6)./2^6;%on and off
                        sigDPFSKtempArrDsss(iSym+1,:) = t_sigDPFSKtempArrDsss_tmp2;%(sigDPFSKtempArrDsss(iSym+1,:) - 2*pi);%
                      else if phaseTmp < -pi
                            t_sigDPFSKtempArrDsss_tmp2 = round((sigDPFSKtempArrDsss(iSym+1,:) + 2*pi).*2^6)./2^6;%on and off
                            sigDPFSKtempArrDsss(iSym+1,:) = t_sigDPFSKtempArrDsss_tmp2;%(sigDPFSKtempArrDsss(iSym+1,:) + 2*pi);%
                        else if phaseTmp < pi && phaseTmp > -pi
                                break;
                            end
                        end
                    end
                end

    %             sigPreArrR = sigDPFSKtempArrDsss(iSym,1:Ri+1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,1);
    %             t_sigPreArrR_tmp = round((sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri)).*2^6)./2^6;
    %             t_sigPreArrL_tmp = round((sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri)).*2^6)./2^6;
                sigPreArrR = sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri);%°ÑÇ°Ò»¸ö·ûºÅÎ²²¿ÍùºóÑÓ³¤Li+1¸öµãÈ»ºóÈ¡ÑÓ³¤ºóµÄÎ²²¿Ri+1¸öµã
                sigPreArrL = sigDPFSKtempArrDsss(iSym+1,1:Ri+1) + sigDPFSKtempArrDsss(iSym+1,Li+1) - sigDPFSKtempArrDsss(iSym+1,Ri+1);%°ÑºóÒ»¸ö·ûºÅÍùÇ°ÑÓ³¤Li+1¸öµãÈ»ºóÈ¡ÑÓ³¤ºóµÄÍ·²¿Ri+1¸öµã

                t_sigDPFSKtempArrTS_tmp = round((sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li)).*2^6)./2^6;%on and off
                sigDPFSKtempArrTS(iSym,LenSym1-HalfWinLen+1:LenSym1) = t_sigDPFSKtempArrTS_tmp;%sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li);%t_sigDPFSKtempArrTS_tmp;%
                t_sigDPFSKtempArrTS_tmp = round((sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li)).*2^6)./2^6;
                sigDPFSKtempArrTS(iSym+1,1:HalfWinLen) = t_sigDPFSKtempArrTS_tmp;%sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li);%t_sigDPFSKtempArrTS_tmp;%

                sigDPFSKtempArrTS(iSym+1,HalfWinLen + 1: LenSym1) = sigDPFSKtempArrDsss(iSym+1,HalfWinLen + 1: LenSym1);
            end

            sig = reshape(sigDPFSKtempArrTS.',1,[]).'; 

            %  figure;
            %  subplot(2,1,1);plot(phase(sigDPFSK));title('phase');
            %  subplot(2,1,2);plot(abs(sigDPFSK));title('abs');
end
if key==2
    LenSym1 = LenSymPhase;
    rollLen = LenSym1*TransRatio ;%  ²ÎÓë¹ý¶ÉµÄ²ÉÑùµã¸öÊýTransRatio =  1/4  1/2  3/4 1 

    HalfWinLen = rollLen/2 -1;
    halfWin =  [1/2+1/2*cos((0:rollLen)*pi/rollLen)];%[1/2+1/2*cos((0:rollLen)*t_tmp)];%
    % WinR = halfWin(1,:);
    % WinL = fliplr(halfWin(1,:));
     WinR = halfWin(1,2:end - 1);%t_halfWin_tmp;%
     WinL = fliplr(WinR);

    Ri = HalfWinLen*2;%Á½±¶´°³¤¶È
    Li = HalfWinLen;%µ¥±¶´°³¤¶È
    Midi = LenSym1 - HalfWinLen;%·ûºÅ³¤¶È¼õÈ¥´°³¤¶È

    % figure;plot(WinR);
    % figure;plot(WinL);
    FskPhase_rap = mod(FskPhase./pi+1,2)-1;
    PskPreamblePhase_rap = mod(PskPreamblePhase./pi + 1,2)-1;
     
    sigPreArr = zeros(SymNumPreamble,LenSym1);
    for iSym=1:SymNumPreamble
        sigPreArr(iSym,:) = mod( FskPhase_rap(:,FskPreamble(iSym)) + PskPreamblePhase_rap(iSym) +1 , 2)-1 ;%t_sigDPFSKtempArr;%
    end


           %Êý¾Ýµ÷ÖÆ 
            %4fsk
            bits1 = intrlvBits1(1:2:end);
            bits2 = intrlvBits1(2:2:end);

            %dqpsk
            bits3 = intrlvBits2(1:2:end);
            bits4 = intrlvBits2(2:2:end);

            Reference = PskPreamblePhase_rap(end); % ×îºóÒ»¸öpreamble·ûºÅµÄÏàÎ»
            sigDPFSKtempArr = zeros(SymNumData+SymNumPreamble,LenSym1);
            sigDPFSKtempArr(1:SymNumPreamble,:) = sigPreArr;

            tmp2_rap = [0 1/2*pi pi -1/2*pi];
            tmp2_rap = mod(tmp2_rap / pi + 1,2)-1;
            aa = zeros(length(bits1));
            for iSym = 1 : SymNumData
                tmp1 =  Graymapping([],[],bits1(iSym),bits2(iSym),Mfsk); % graymapping 00 1  01 2  11 3   10 4
                tmp2 =  Graymapping([],[],bits3(iSym),bits4(iSym),Mfsk);%00 0   01 1/2pi 11 pi  10 -1/2pi
                tmp2phase = mod(Reference + tmp2_rap(tmp2) + 1  ,2)-1;%0   0.5   1   -0.5
                sigDPFSKtempArr(iSym + SymNumPreamble , 1 : LenSym1 )= mod(FskPhase_rap(:,tmp1) + tmp2phase + 1,2) -1;%t_sigDPFSKtempArr_tmp;%
                Reference = tmp2phase; % 
            end

            % DSSS 
            DSeqPhase = phase(DSeq);
            DSeqPhase_rap  = mod(DSeqPhase./pi + 1,2) - 1;
            sigDPFSKtempArrDsss = zeros( (SymNumData+SymNumPreamble) * DSSSNum + 1, LenSym1);
            for i = 1:SymNumData+SymNumPreamble
                sigDPFSKtempArrDsss((i-1)*DSSSNum+1:i*DSSSNum,1:LenSym1) = mod(sigDPFSKtempArr(i,1:LenSym1)+DSeqPhase_rap.' + 1,2)-1;%t_sigDPFSKtempArrDsss_tmp;%
            end

            % 
            sigDPFSKtempArrDsss((SymNumData+SymNumPreamble)*DSSSNum+1,:) = sigPreArr(1,:);
            if iBlock > 1  % °ü¿éÊý´óÓÚ2²Å´¦Àí
                sigDPFSKtempArrDsss(1,:) = FirstTransiPreamble.';
            end
if TransRatio ~= 0
            % Transition 
            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            for ii = 1:(SymNumData+SymNumPreamble) * DSSSNum + 1
                for jj = 1:LenSym1 -1
                    tmp = sigDPFSKtempArrDsss(ii,jj +1) - sigDPFSKtempArrDsss(ii,jj);
                    if tmp > 1
                     sigDPFSKtempArrDsss(ii,jj+1) = sigDPFSKtempArrDsss(ii,jj+1) - 2*(fix(abs(tmp/2))+1);
                    end
                     if tmp < -1
                     sigDPFSKtempArrDsss(ii,jj+1) = sigDPFSKtempArrDsss(ii,jj+1) + 2*(fix(abs(tmp/2))+1);
                    end
                end
            end
            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            sigDPFSKtempArrTS = zeros( (SymNumData+SymNumPreamble) * DSSSNum, LenSym1);
            sigDPFSKtempArrTS(1,1:LenSym1-HalfWinLen ) = mod(sigDPFSKtempArrDsss(1,1: LenSym1-HalfWinLen ) + 1,2) -1;
            for iSym= 1 : (SymNumData+SymNumPreamble)*DSSSNum 
                move_count = 0;
                while(1)
                    phaseTmp = sigDPFSKtempArrDsss(iSym+1,1) - sigDPFSKtempArrDsss(iSym,LenSym1);%sigPreArrPhase  sigPreArr(iSym,LenSym1)
                    if phaseTmp > 1
                        sigDPFSKtempArrDsss(iSym+1,:) = sigDPFSKtempArrDsss(iSym+1,:) - 2;%t_sigDPFSKtempArrDsss_tmp2;%
                        move_count = move_count +1;
                      else if phaseTmp < -1
                            sigDPFSKtempArrDsss(iSym+1,:) = sigDPFSKtempArrDsss(iSym+1,:) + 2;%t_sigDPFSKtempArrDsss_tmp2;%
                            move_count = move_count - 1;
                        else if phaseTmp < 1 && phaseTmp > -1
                                break;
                            end
                        end
                    end
                end

    %             sigPreArrR = sigDPFSKtempArrDsss(iSym,1:Ri+1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,1);
                sigPreArrR = sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri);%mod(sigDPFSKtempArrDsss(iSym,LenSym1-Ri:LenSym1) + sigDPFSKtempArrDsss(iSym,Midi+1) - sigDPFSKtempArrDsss(iSym,LenSym1-Ri) +1 ,2)-1 ;
                sigPreArrL = sigDPFSKtempArrDsss(iSym+1,1:Ri+1) + sigDPFSKtempArrDsss(iSym+1,Li+1) - sigDPFSKtempArrDsss(iSym+1,Ri+1);%mod(sigDPFSKtempArrDsss(iSym+1,1:Ri+1) + sigDPFSKtempArrDsss(iSym+1,Li+1) - sigDPFSKtempArrDsss(iSym+1,Ri+1) +1 ,2) -1;

                sigDPFSKtempArrTS(iSym,LenSym1-HalfWinLen+1:LenSym1) = mod(sigPreArrR(1,1:Li).*WinR(1,1:Li) + sigPreArrL(1,1:Li).*WinL(1,1:Li) + 1,2)-1 ;
                sigDPFSKtempArrTS(iSym+1,1:HalfWinLen) = mod(sigPreArrR(1,1+Li:Li+Li).*WinR(1,1+Li:Li+Li) + sigPreArrL(1,1+Li:Li+Li).*WinL(1,1+Li:Li+Li) + 1,2)-1;

                sigDPFSKtempArrTS(iSym+1,HalfWinLen + 1: LenSym1) = mod(sigDPFSKtempArrDsss(iSym+1,HalfWinLen + 1: LenSym1) + 1,2) - 1;
                
                sigDPFSKtempArrDsss(iSym+1,:) = sigDPFSKtempArrDsss(iSym+1,:)  + 2*move_count;
            end
else
     sigDPFSKtempArrTS = sigDPFSKtempArrDsss;
end

            sig = reshape(sigDPFSKtempArrTS.',1,[]).'; 

            %  figure;
            %  subplot(2,1,1);plot(phase(sigDPFSK));title('phase');
            %  subplot(2,1,2);plot(abs(sigDPFSK));title('abs');
    
    
end

end