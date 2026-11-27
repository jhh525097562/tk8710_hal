function write2data(varargin)

%% Inital
bw      =   12  ;
inter   =   1   ;
frac    =   20  ;
if(nargin < 1)
    disp('Error');
    return;
else
    in          =   varargin{1};
    filename    =   varargin{2}; 
end

if(nargin > 1)
    for ck0 = 1 : 1 : nargin - 1
        if(strcmp(varargin{ck0 + 1}, 'txt'))
            format  =   'txt';
            if(ck0 + 2 < nargin)
                inter   =   varargin{ck0 + 2};
                frac    =	varargin{ck0 + 3};
            end
        elseif(strcmp(varargin{ck0 + 1}, 'rtl'))
            format  =   'rtl';
            if(ck0 + 2 < nargin)
                bw      =   varargin{ck0 + 2};
                inter   =	varargin{ck0 + 3};
            end
        end
    end
end

fid  = fopen(filename, 'w');
imag_en     =   ~isreal(in);
I = real(in);
Q = imag(in);
I_rtl = floor(I * 2^(bw - inter));
Q_rtl = floor(Q * 2^(bw - inter));
if(bw == 1 && inter == 1)
    I_rtl(I_rtl > 0) = 0;
    Q_rtl(Q_rtl > 0) = 0;
    I_rtl(I_rtl < 0) = 1;
    Q_rtl(Q_rtl < 0) = 1;
else
    I_rtl(I_rtl < 0) = I_rtl(I_rtl < 0) + 2^bw;
    Q_rtl(Q_rtl < 0) = Q_rtl(Q_rtl < 0) + 2^bw;
end

if(imag_en)
    if(strcmp(format, 'txt'))
        o_format    =   ['%' num2str(inter) '.' num2str(frac) 'f\t%' num2str(inter) '.' num2str(frac) 'f\n'];
%         o_format    =   ['%2.10f\t%2.10f\n' ] ;
        out_I       =   I                       ;
        out_Q       =   Q                       ;
    elseif(strcmp(format, 'rtl'))
        format_len = ceil(bw / 4);
        o_format    =   ['%0' num2str(format_len) 'x' '%0' num2str(format_len) 'x\n' ];
        out_I       =   I_rtl                   ;
        out_Q       =   Q_rtl                   ;
    end
    for ck0 = 1 : 1 : length(I)
        fprintf(fid, o_format, out_I(ck0), out_Q(ck0));
    end
else
    if(strcmp(format, 'txt'))
        o_format    =   ['%' num2str(inter) '.' num2str(frac) 'f\n'];
        out_I       =   I                       ;
    elseif(strcmp(format, 'rtl'))
        format_len = ceil(bw / 4);
        o_format    =   ['0x%0' num2str(format_len) 'x,'];
        out_I       =   I_rtl                   ;
    end
    for ck0 = 1 : 1 : length(I)
    fprintf(fid, o_format, out_I(ck0));
    end
end
fclose(fid);
end
