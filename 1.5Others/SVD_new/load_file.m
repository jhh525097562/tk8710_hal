function data = load_file(varargin)

%% Inital
plot_en     =   0   ;
dat_mode    =   0   ;
bw          =   12  ;
inter       =   1   ;
fs          =   []  ;
ant_n       =   9   ;
mode        =   1   ;
if(nargin < 1)
    disp('Error');
    return;
else
    filename = varargin{1};
end
if(nargin > 1)
    for ck0 = 1 : 1 : nargin - 1
        if(strcmp(varargin{ck0 + 1}, 'plot'))
            plot_en = 1;
        end
        if(strcmp(varargin{ck0 + 1}, 'fs'))
            fs = varargin{ck0 + 2};
        end
        if(strcmp(varargin{ck0 + 1}, 'ant_n'))
            ant_n = varargin{ck0 + 2};
        end
        if(strcmp(varargin{ck0 + 1}, 'cap'))
            dat_mode  =   1   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'fft_power'))
            dat_mode  =   4   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'dsp'))
            dat_mode  =   2   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'gpio'))
            dat_mode  =   3   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'gpio_gw'))
            dat_mode  =   7   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'gpio_sdm'))
            dat_mode  =   8   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'sigma'))
            dat_mode  =   6   ;
        end
        if(strcmp(varargin{ck0 + 1}, 'rtl'))
            dat_mode  =   0   ;
            bw      =   varargin{ck0 + 2};
            inter   =	varargin{ck0 + 3};
        end
        if(strcmp(varargin{ck0 + 1}, 'mean'))
            dat_mode  =   5   ;
            bw      =   varargin{ck0 + 2};
            inter   =	varargin{ck0 + 3};
        end
        if(strcmp(varargin{ck0 + 1}, 'mode'))
            mode    =   varargin{ck0 + 2};
        end
    end
end

%% Load File
[~, ~, ext] = fileparts(filename);
if(strcmp(ext, '.txt'))
    in          =   load(filename);
    [~, col]    =   size(in);
    if(col == 2)
        data    =   transpose(in(:, 1) + sqrt(-1) * in(:, 2));
    elseif(col == 1)
        data    =   transpose(in);
    end
elseif(strcmp(ext, '.csv'))
    if(dat_mode == 3)
        Path    =   './';
        file    =   dir(fullfile(Path,'*.csv'));
        for ck0 = 1 : 1 : length(file)
            if(strcmp(file(ck0).name(end - 4), 'I')||strcmp(file(ck0).name(end - 4), 'i'))
                I = csvread(file(ck0).name, 1, 0);
            end
            if(strcmp(file(ck0).name(end - 4), 'Q')||strcmp(file(ck0).name(end - 4), 'q'))
                Q = csvread(file(ck0).name, 1, 0);
            end
        end
        I = I(:, 3);
        Q = Q(:, 3);
        in_I = I;
        in_Q = Q;
        %         in_I = in_I - 2^15;
        %         in_Q = in_Q - 2^15;
        for ck0 = 1 : 1 : length(in_I)
            if(in_I(ck0) > 2^15)
                in_I(ck0) = in_I(ck0) - 2^16;
            end
        end
        for ck0 = 1 : 1 : length(in_Q)
            if(in_Q(ck0) > 2^15)
                in_Q(ck0) = in_Q(ck0) - 2^16;
            end
        end
        if(length(in_I) >=  length(in_Q))
            in = in_I(1:length(in_Q)) + sqrt(-1) * in_Q;
        else
            in = in_I + sqrt(-1) * in_Q(1:length(in_I));
        end
        data = transpose(in / 1);% 2^15);
    elseif(dat_mode == 6)
        Path    =   './';
        file    =   dir(fullfile(Path,'*.csv'));
        for ck0 = 1 : 1 : length(file)
            if(strcmp(file(ck0).name(end - 4), 'I')||strcmp(file(ck0).name(end - 4), 'i'))
                I = csvread(file(ck0).name, 1, 0);
            end
            if(strcmp(file(ck0).name(end - 4), 'Q')||strcmp(file(ck0).name(end - 4), 'q'))
                Q = csvread(file(ck0).name, 1, 0);
            end
        end
        I = I(:, 3);
        Q = Q(:, 3);
        in_I = I;
        in_Q = Q;
        cnt = 1;
        for ck1 = 31 : -1 : 0
            in_I_bit(:, cnt) = bitand(bitshift(in_I, ck1 * -1), 1);
            in_Q_bit(:, cnt) = bitand(bitshift(in_Q, ck1 * -1), 1);
            cnt = cnt + 1;
        end
        in_i = reshape(transpose(in_I_bit),1,[]);
        in_q = reshape(transpose(in_Q_bit),1,[]);
        in = in_i + sqrt(-1) * in_q;
        data = transpose(in / 1);% 2^15);
    elseif(dat_mode == 7)
        data_tmp = csvread(filename, 1, 0);
        data_tmp = data_tmp(:, 3);
        Q = mod(data_tmp, 2^16);
        I = floor(data_tmp / 2^16);
        I(I > 2^12) = I(I > 2^12) - 2^13;
        Q(Q > 2^12) = Q(Q > 2^12) - 2^13;
        data = I + Q * sqrt(-1);
        data = transpose(data);
    elseif(dat_mode == 8)
        data_tmp = csvread(filename, 1, 0);
        data_tmp = data_tmp(:, 3);
        out_tmp = int2bit(data_tmp, 32, 0);
        out_tmp(out_tmp == 0) = -1;
        data = transpose(out_tmp);
    end
elseif(strcmp(ext, '.dat')||strcmp(ext, '.DAT'))
    if(dat_mode == 1)
        fp          =   fopen(filename, 'rb');
        in          =   fread(fp, 4096*3,'uint8');
        len         =   3 * floor(length(in)  / 3 );
        fclose(fp);
        in          =   reshape(in(1:len), 3, []);
        data_dec    =   in(3,:)*2^16 + in(2,:)*2^8 + in(1,:);
        bw_sp       =   length(dec2hex(data_dec)) / 2 * 4;
    elseif(dat_mode == 0 ||dat_mode == 4 || dat_mode == 5)
        data        =   textread(filename, '%s');

        data_dec    =   hex2dec(data);
        bw_sp       =   length(data{1}) / 2* 4;
        % data_dec        =   dat_read(filename);
        % bw_sp       =   bw;%length(data{1}) / 2 * 4;
    elseif(dat_mode == 2)
        fp          =   fopen(filename,'r');
        data_tmp    =   fread(fp,[18 25000],'float');
        data        =   zeros(9, 25000);
        for i=1:9
            data(i,:) = data_tmp(2*(i-1)+1,:) + data_tmp(2*i,:)*1i;
        end
    end
    if(dat_mode == 1 || dat_mode == 0 || dat_mode == 4||dat_mode == 5)
        if(dat_mode == 0)
            in_q        =   US2TC(mod(data_dec,2^bw_sp),bw );
            in_i        =   US2TC(floor(data_dec/2^bw_sp),bw);
            data        =   (in_i + sqrt(-1) * in_q) / 2^(bw - inter);
%             in_q(in_q > 2^24) =  in_q(in_q > 2^24) - 2^25;
%             data = in_q / 2^24;
        elseif(dat_mode == 1)
            in_q        =   US2TC(mod(data_dec,2^bw_sp),bw);
            in_i        =   US2TC(floor(data_dec/2^bw_sp),bw);
            data        =   (in_i + sqrt(-1) * in_q) / 2^(bw - inter);
        elseif(dat_mode == 4)
            data        =   transpose(data_dec);
        elseif(dat_mode == 5)
            in_q        =   mod(data_dec,2^bw_sp);
            in_i        =   floor(data_dec/2^bw_sp);
            in_i        =   in_i - mean(in_i);
            in_q        =   in_q - mean(in_q);
            data        =   (in_i + sqrt(-1) * in_q) / 2^(bw - inter);
        end
    end
elseif(strcmp(ext, '.mat'))
    in          =   load(filename);
    data        =   getfield(in, char(fieldnames(in)));
end

%% Plot
if(strcmp(ext, '.dat') && dat_mode == 2)
    data_plot   =   data(ant_n, :);
else
    data_plot   =   data;
end

if(plot_en)
    figure(1);
    plot(real(data_plot));
    hold on;
    plot(imag(data_plot));
    [Pxif1, fif1] = pwelch(data_plot, [], [], [], fs);figure(2);plot(fif1,10*log10(abs(Pxif1))); grid on;
end
end

function data = dat_read(filename)
fid = fopen(filename);
temp = fgetl(fid);
data = [];
while(ischar(temp))
    if(length(temp) == 8 || length(temp) == 55)
        if(length(temp) == 55)
            temp = textscan(temp, '%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x');
        else
            temp =textscan(temp, '%x');
        end
        temp = double(cell2mat(temp));
        data = [data, temp];
    end
    temp = fgetl(fid);
end
fclose(fid);
end

