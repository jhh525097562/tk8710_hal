% MATLAB脚本：读取TK8710采集的天线数据并转换为复数
% 功能：读取8根天线的二进制数据文件，转换为复数格式
% 数据格式：16bit有符号整数，1bit整数位，15bit小数位
% 复数格式：每16384个16bit数据 = 8192个复数（I/Q交替）

function read_tk8710_capture_data()
    clc;
    clear;
    close all;
    
    % 配置参数
    data_dir = '8710CaptureData';
    num_antennas = 8;
    samples_per_antenna = 16384;  % mode5-8: 16384, mode9: 4096, mode10: 2048, mode11/18: 1024
    complex_samples = samples_per_antenna / 2;  % 复数个数
    
    % 数据格式参数
    data_type = 'int16';  % 16bit有符号整数
    scale_factor = 2^15;  % 15bit小数位，转换为浮点数时的缩放因子
    
    fprintf('=== TK8710采集数据读取脚本 ===\n');
    fprintf('数据目录: %s\n', data_dir);
    fprintf('天线数量: %d\n', num_antennas);
    fprintf('每根天线采样点数: %d\n', samples_per_antenna);
    fprintf('复数个数: %d\n', complex_samples);
    fprintf('数据格式: %s, 1bit整数+15bit小数\n\n', data_type);
    
    % 检查目录是否存在
    if ~exist(data_dir, 'dir')
        error('数据目录不存在: %s', data_dir);
    end
    
    % 初始化存储数组
    antenna_data = cell(num_antennas, 1);  % 存储原始数据
    complex_data = cell(num_antennas, 1); % 存储复数数据
    
    % 读取每根天线的数据
    for antenna_idx = 1:num_antennas
        % 构造文件名
        filename = sprintf('%s/AntennaData%d.bin', data_dir, antenna_idx);
        
        % 检查文件是否存在
        if ~exist(filename, 'file')
            warning('文件不存在: %s', filename);
            continue;
        end
        
        fprintf('读取天线%d数据: %s\n', antenna_idx, filename);
        
        % 读取二进制数据
        fid = fopen(filename, 'r');
        if fid == -1
            error('无法打开文件: %s', filename);
        end
        
        % 读取16bit有符号整数数据
        raw_data = fread(fid, inf, data_type);
        fclose(fid);
        
        % 检查数据长度
        if length(raw_data) ~= samples_per_antenna
            warning('天线%d数据长度不匹配: 期望%d，实际%d', ...
                    antenna_idx, samples_per_antenna, length(raw_data));
        end
        
        % 存储原始数据
        antenna_data{antenna_idx} = raw_data;
        
        % 转换为浮点数（除以2^15）
        float_data = double(raw_data) / scale_factor;
        
        % 转换为复数（I/Q交替）
        % 假设数据格式：I0, Q0, I1, Q1, I2, Q2, ...
        % 重塑为2列矩阵：[I, Q]
        iq_data = reshape(float_data, 2, [])';
        
        % 创建复数
        complex_signal = complex(iq_data(:, 1), iq_data(:, 2));
        
        % 存储复数数据
        complex_data{antenna_idx} = complex_signal;
        
        fprintf('  - 原始数据点数: %d\n', length(raw_data));
        fprintf('  - 复数点数: %d\n', length(complex_signal));
        fprintf('  - 数据范围: [%.6f, %.6f]\n', min(float_data), max(float_data));
        fprintf('  - 复数幅度范围: [%.6f, %.6f]\n\n', ...
                min(abs(complex_signal)), max(abs(complex_signal)));
    end
    
    % 数据分析和可视化
    analyze_and_plot_data(antenna_data, complex_data, num_antennas);
    
    % 保存处理后的数据
    save_processed_data(complex_data, num_antennas);
    
    fprintf('=== 数据处理完成 ===\n');
end

function analyze_and_plot_data(antenna_data, complex_data, num_antennas)
    fprintf('\n=== 数据分析和可视化 ===\n');
    
    % 创建图形窗口
    figure('Position', [100, 100, 1200, 800]);
    
    % 绘制每根天线的时域波形
    for antenna_idx = 1:min(num_antennas, 4)  % 只显示前4根天线
        if isempty(complex_data{antenna_idx})
            continue;
        end
        
        % 时域波形 - 实部
        subplot(4, 2, (antenna_idx-1)*2 + 1);
        plot(real(complex_data{antenna_idx}(1:1000)));  % 只显示前1000个点
        title(sprintf('天线%d - 实部 (I)', antenna_idx));
        xlabel('采样点');
        ylabel('幅度');
        grid on;
        
        % 时域波形 - 虚部
        subplot(4, 2, (antenna_idx-1)*2 + 2);
        plot(imag(complex_data{antenna_idx}(1:1000)));  % 只显示前1000个点
        title(sprintf('天线%d - 虚部 (Q)', antenna_idx));
        xlabel('采样点');
        ylabel('幅度');
        grid on;
    end
    
    sgtitle('TK8710天线数据时域波形');
    
    % 绘制频谱分析
    figure('Position', [200, 200, 1200, 600]);
    
    for antenna_idx = 1:min(num_antennas, 4)
        if isempty(complex_data{antenna_idx})
            continue;
        end
        
        subplot(2, 2, antenna_idx);
        
        % 计算频谱
        signal = complex_data{antenna_idx};
        nfft = 2^nextpow2(length(signal));
        fft_data = fft(signal, nfft);
        fft_mag = abs(fft_data(1:nfft/2));
        freq_axis = (0:nfft/2-1) / nfft;
        
        % 绘制频谱（dB）
        plot(freq_axis, 20*log10(fft_mag/max(fft_mag)));
        title(sprintf('天线%d - 频谱', antenna_idx));
        xlabel('归一化频率');
        ylabel('幅度 (dB)');
        grid on;
    end
    
    sgtitle('TK8710天线数据频谱分析');
    
    % 绘制星座图
    figure('Position', [300, 300, 800, 600]);
    
    for antenna_idx = 1:min(num_antennas, 4)
        if isempty(complex_data{antenna_idx})
            continue;
        end
        
        subplot(2, 2, antenna_idx);
        
        % 绘制星座图（只显示前1000个点）
        signal = complex_data{antenna_idx}(1:1000);
        scatter(real(signal), imag(signal), 2, 'filled');
        title(sprintf('天线%d - 星座图', antenna_idx));
        xlabel('实部 (I)');
        ylabel('虚部 (Q)');
        grid on;
        axis equal;
    end
    
    sgtitle('TK8710天线数据星座图');
    
    % 统计信息
    fprintf('\n=== 统计信息 ===\n');
    for antenna_idx = 1:num_antennas
        if isempty(complex_data{antenna_idx})
            continue;
        end
        
        signal = complex_data{antenna_idx};
        fprintf('天线%d:\n', antenna_idx);
        fprintf('  - 平均功率: %.6f\n', mean(abs(signal).^2));
        fprintf('  - 峰值功率: %.6f\n', max(abs(signal).^2));
        fprintf('  - 信噪比估计: %.2f dB\n', 10*log10(mean(abs(signal).^2) / var(abs(signal))));
    end
end

function save_processed_data(complex_data, num_antennas)
    fprintf('\n=== 保存处理后的数据 ===\n');
    
    % 创建MAT数据文件
    save_filename = 'tk8710_processed_data.mat';
    
    % 准备保存的数据结构
    processed_data = struct();
    
    for antenna_idx = 1:num_antennas
        if ~isempty(complex_data{antenna_idx})
            field_name = sprintf('antenna_%d', antenna_idx);
            processed_data.(field_name) = complex_data{antenna_idx};
        end
    end
    
    % 保存数据
    save(save_filename, 'processed_data');
    fprintf('已保存处理后的数据到: %s\n', save_filename);
    
    % 保存为CSV格式（便于其他工具使用）
    for antenna_idx = 1:num_antennas
        if isempty(complex_data{antenna_idx})
            continue;
        end
        
        csv_filename = sprintf('antenna_%d_complex_data.csv', antenna_idx);
        signal = complex_data{antenna_idx};
        
        % 创建CSV数据：实部,虚部,幅度,相位
        csv_data = [real(signal), imag(signal), abs(signal), angle(signal)];
        
        % 写入CSV文件
        fid = fopen(csv_filename, 'w');
        fprintf(fid, 'Real,Imag,Magnitude,Phase\n');
        for i = 1:size(csv_data, 1)
            fprintf(fid, '%.6f,%.6f,%.6f,%.6f\n', csv_data(i, 1), csv_data(i, 2), csv_data(i, 3), csv_data(i, 4));
        end
        fclose(fid);
        
        fprintf('已保存天线%d数据到: %s\n', antenna_idx, csv_filename);
    end
end

% 运行主函数
if ~exist('read_tk8710_capture_data', 'file')
    % 如果直接运行此脚本，调用主函数
    read_tk8710_capture_data();
end
