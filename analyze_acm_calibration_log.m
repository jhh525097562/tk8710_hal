function results = analyze_acm_calibration_log(log_file, output_dir)
%ANALYZE_ACM_CALIBRATION_LOG Extract and analyze periodic ACM calibrations.
%   RESULTS = ANALYZE_ACM_CALIBRATION_LOG() reads
%   test/CaliFactor/CaliFactor.txt relative to this script.
%
%   RESULTS = ANALYZE_ACM_CALIBRATION_LOG(LOG_FILE, OUTPUT_DIR) writes:
%     acm_periodic_calibrations.csv  - one row per periodic calibration
%     acm_channel_statistics.csv     - periodic per-channel statistics
%     acm_all_channel_statistics.csv - all-result per-channel statistics
%     acm_calibration_trends.png     - periodic I/Q trends
%     acm_all_calibration_scatter.png - all-result I/Q scatter
%     acm_all_variance_magnitude.png  - variance and magnitude mean
%
%   A periodic calibration starts at the TRM "starts at last-frame S2 end"
%   message.  This intentionally excludes the startup count=100 batch.

    script_dir = fileparts(mfilename('fullpath'));
    if nargin < 1 || isempty(log_file)
        log_file = fullfile(script_dir, 'test', 'CaliFactor', 'CaliFactor.txt');
    end
    if nargin < 2 || isempty(output_dir)
        output_dir = fullfile(fileparts(log_file), 'analysis');
    end
    if ~isfile(log_file)
        error('Log file does not exist: %s', log_file);
    end
    if ~isfolder(output_dir)
        mkdir(output_dir);
    end

    text = fileread(log_file);
    lines = regexp(text, '\r\n|\n|\r', 'split');
    [records, startup] = parse_log(lines);
    all_factors = parse_all_factor_blocks(lines);
    if isempty(records)
        error('No periodic ACM calibration was found in: %s', log_file);
    end

    results = records_to_table(records);
    csv_file = fullfile(output_dir, 'acm_periodic_calibrations.csv');
    writetable(results, csv_file);

    stats = calculate_channel_statistics(results);
    stats_file = fullfile(output_dir, 'acm_channel_statistics.csv');
    writetable(stats, stats_file);

    all_stats = calculate_factor_statistics(all_factors.i, all_factors.q);
    writetable(all_stats, fullfile(output_dir, 'acm_all_channel_statistics.csv'));

    plot_trends(results, fullfile(output_dir, 'acm_calibration_trends.png'));
    plot_all_scatter(all_factors, fullfile(output_dir, 'acm_all_calibration_scatter.png'));
    plot_variance_magnitude(all_stats, fullfile(output_dir, 'acm_all_variance_magnitude.png'));
    print_summary(results, stats, startup, log_file, output_dir);
    fprintf('All factor blocks used for plots: %d (startup=%d, periodic=%d)\n', ...
        size(all_factors.i, 1), size(all_factors.i, 1) - height(results), height(results));
end

function factors = parse_all_factor_blocks(lines)
    % Every factor header represents one usable calibration result.  This
    % includes both the startup batch and later periodic calibrations.
    factors.i = zeros(0, 8);
    factors.q = zeros(0, 8);
    collecting = false;
    current_i = nan(1, 8);
    current_q = nan(1, 8);
    seen = false(1, 8);
    for line_no = 1:numel(lines)
        line = lines{line_no};
        if contains(line, '=== ACM calibration factors ===')
            if collecting && all(seen)
                factors.i(end + 1, :) = current_i; %#ok<AGROW>
                factors.q(end + 1, :) = current_q; %#ok<AGROW>
            end
            current_i = nan(1, 8);
            current_q = nan(1, 8);
            seen = false(1, 8);
            collecting = true;
            continue;
        end
        if ~collecting, continue; end
        token = regexp(line, 'Channel(\d+): I=0x([0-9A-Fa-f]+).*Q=0x([0-9A-Fa-f]+)', 'tokens', 'once');
        if ~isempty(token)
            channel = str2double(token{1}) + 1;
            if channel >= 1 && channel <= 8
                current_i(channel) = signed18(hex2dec(token{2})) / 2^15;
                current_q(channel) = signed18(hex2dec(token{3})) / 2^15;
                seen(channel) = true;
            end
            if all(seen)
                factors.i(end + 1, :) = current_i; %#ok<AGROW>
                factors.q(end + 1, :) = current_q; %#ok<AGROW>
                collecting = false;
            end
        end
    end
end

function [records, startup] = parse_log(lines)
    records = repmat(empty_record(), 0, 1);
    current = empty_record();
    active = false;
    startup.target = NaN;
    startup.valid = NaN;

    for line_no = 1:numel(lines)
        line = lines{line_no};

        token = regexp(line, 'Start ACM calibration \d+ \(target count: (\d+), SNR threshold: (\d+)\)', 'tokens', 'once');
        if ~isempty(token)
            startup.target = str2double(token{1});
        end
        token = regexp(line, 'ACM calibration completed, valid count: (\d+)', 'tokens', 'once');
        if ~isempty(token)
            startup.valid = str2double(token{1});
        end

        token = regexp(line, 'ACM calibration starts at last-frame S2 end, slot3 window=(\d+) us, s0PeriodBefore=(\d+) us, s0CountBefore=(\d+)', 'tokens', 'once');
        if ~isempty(token)
            if active
                records(end + 1, 1) = current; %#ok<AGROW>
            end
            current = empty_record();
            current.index = numel(records) + 1;
            current.start_line = line_no;
            current.timestamp_ms = parse_timestamp(line);
            current.slot3_window_us = str2double(token{1});
            current.s0_period_before_us = str2double(token{2});
            current.s0_count_before = str2double(token{3});
            active = true;
            continue;
        end

        if ~active
            continue;
        end

        token = regexp(line, 'Channel(\d+): I=0x([0-9A-Fa-f]+) \(([-+0-9.eE]+)\), Q=0x([0-9A-Fa-f]+) \(([-+0-9.eE]+)\)', 'tokens', 'once');
        if ~isempty(token)
            channel = str2double(token{1}) + 1;
            if channel >= 1 && channel <= 8
                current.i_hex{channel} = upper(token{2});
                current.q_hex{channel} = upper(token{4});
                % Convert the raw 18-bit values instead of using the rounded
                % three-decimal values printed in the log.
                current.i(channel) = signed18(hex2dec(token{2})) / 2^15;
                current.q(channel) = signed18(hex2dec(token{4})) / 2^15;
            end
            continue;
        end

        if contains(line, 'TRM: ACM hidden in slot3:')
            current.valid = field_number(line, 'valid');
            current.elapsed_us = field_number(line, 'elapsed');
            current.wait_us = field_number(line, 'wait');
            current.trigger_late_us = field_number(line, 'triggerLate');
            current.trigger_cost_us = field_number(line, 'triggerCost');
            current.s0_period_after_us = field_number(line, 's0PeriodAfterStart');
            current.s0_count_after = field_number(line, 's0CountAfterStart');
            continue;
        end

        token = regexp(line, 'ACM result published to RAM bank=(\d+) generation=(\d+) valid=(\d+)', 'tokens', 'once');
        if ~isempty(token)
            current.ram_bank = str2double(token{1});
            current.generation = str2double(token{2});
            current.publish_valid = str2double(token{3});
            current.end_line = line_no;
            records(end + 1, 1) = current; %#ok<AGROW>
            active = false;
        end
    end

    % Keep an interrupted/incomplete attempt visible instead of silently
    % discarding it.  Complete records have a finite generation value.
    if active
        records(end + 1, 1) = current;
    end
end

function record = empty_record()
    record.index = NaN;
    record.start_line = NaN;
    record.end_line = NaN;
    record.timestamp_ms = NaN;
    record.slot3_window_us = NaN;
    record.s0_period_before_us = NaN;
    record.s0_count_before = NaN;
    record.i = nan(1, 8);
    record.q = nan(1, 8);
    record.i_hex = repmat({''}, 1, 8);
    record.q_hex = repmat({''}, 1, 8);
    record.valid = NaN;
    record.elapsed_us = NaN;
    record.wait_us = NaN;
    record.trigger_late_us = NaN;
    record.trigger_cost_us = NaN;
    record.s0_period_after_us = NaN;
    record.s0_count_after = NaN;
    record.ram_bank = NaN;
    record.generation = NaN;
    record.publish_valid = NaN;
end

function value = signed18(value)
    value = bitand(uint32(value), uint32(hex2dec('3FFFF')));
    value = double(value);
    if value >= 2^17
        value = value - 2^18;
    end
end

function value = parse_timestamp(line)
    token = regexp(line, '^\[?(\d+)\]?', 'tokens', 'once');
    if isempty(token)
        value = NaN;
    else
        value = str2double(token{1});
    end
end

function value = field_number(line, name)
    token = regexp(line, [name '=(-?\d+)'], 'tokens', 'once');
    if isempty(token)
        value = NaN;
    else
        value = str2double(token{1});
    end
end

function table_out = records_to_table(records)
    n = numel(records);
    scalar_names = {'index', 'start_line', 'end_line', 'timestamp_ms', ...
        'slot3_window_us', 's0_period_before_us', 's0_count_before', ...
        'valid', 'elapsed_us', 'wait_us', 'trigger_late_us', ...
        'trigger_cost_us', 's0_period_after_us', 's0_count_after', ...
        'ram_bank', 'generation', 'publish_valid'};
    table_out = table();
    for k = 1:numel(scalar_names)
        name = scalar_names{k};
        table_out.(name) = reshape([records.(name)], n, 1);
    end
    i_matrix = reshape([records.i], 8, n)';
    q_matrix = reshape([records.q], 8, n)';
    for channel = 0:7
        c = channel + 1;
        i_hex_values = strings(n, 1);
        q_hex_values = strings(n, 1);
        for row = 1:n
            i_hex_values(row) = string(records(row).i_hex{c});
            q_hex_values(row) = string(records(row).q_hex{c});
        end
        table_out.(sprintf('ch%d_i_hex', channel)) = i_hex_values;
        table_out.(sprintf('ch%d_q_hex', channel)) = q_hex_values;
        i_values = i_matrix(:, c);
        q_values = q_matrix(:, c);
        table_out.(sprintf('ch%d_i', channel)) = i_values;
        table_out.(sprintf('ch%d_q', channel)) = q_values;
        table_out.(sprintf('ch%d_magnitude', channel)) = hypot(i_values, q_values);
        table_out.(sprintf('ch%d_phase_deg', channel)) = atan2d(q_values, i_values);
    end
end

function stats = calculate_channel_statistics(results)
    complete = isfinite(results.generation);
    valid = complete & results.valid == 1 & results.publish_valid == 1;
    channel = (0:7)';
    count = zeros(8, 1);
    i_mean = nan(8, 1); i_std = nan(8, 1); i_min = nan(8, 1); i_max = nan(8, 1);
    q_mean = nan(8, 1); q_std = nan(8, 1); q_min = nan(8, 1); q_max = nan(8, 1);
    magnitude_mean = nan(8, 1); magnitude_std = nan(8, 1);
    phase_mean_deg = nan(8, 1); phase_std_deg = nan(8, 1);
    for c = 0:7
        i = results.(sprintf('ch%d_i', c))(valid);
        q = results.(sprintf('ch%d_q', c))(valid);
        good = isfinite(i) & isfinite(q);
        i = i(good); q = q(good);
        count(c + 1) = numel(i);
        if isempty(i), continue; end
        magnitude = hypot(i, q);
        phase = atan2d(q, i);
        i_mean(c + 1) = mean(i); i_std(c + 1) = std(i); i_min(c + 1) = min(i); i_max(c + 1) = max(i);
        q_mean(c + 1) = mean(q); q_std(c + 1) = std(q); q_min(c + 1) = min(q); q_max(c + 1) = max(q);
        magnitude_mean(c + 1) = mean(magnitude); magnitude_std(c + 1) = std(magnitude);
        phase_mean_deg(c + 1) = mean(phase); phase_std_deg(c + 1) = std(phase);
    end
    stats = table(channel, count, i_mean, i_std, i_min, i_max, q_mean, q_std, q_min, q_max, ...
        magnitude_mean, magnitude_std, phase_mean_deg, phase_std_deg);
end

function plot_trends(results, filename)
    valid = results.valid == 1 & results.publish_valid == 1 & isfinite(results.generation);
    fig = figure('Visible', 'off', 'Name', 'ACM calibration I/Q trends', 'Position', [100 100 1400 900]);
    for c = 0:7
        subplot(4, 2, c + 1);
        plot(results.index(valid), results.(sprintf('ch%d_i', c))(valid), '.-', 'DisplayName', 'I'); hold on;
        plot(results.index(valid), results.(sprintf('ch%d_q', c))(valid), '.-', 'DisplayName', 'Q');
        title(sprintf('Channel %d', c)); grid on;
        if c >= 6, xlabel('Periodic calibration index'); end
        ylabel('Factor');
        if c == 0, legend('Location', 'best'); end
    end
    saveas(fig, filename);
    close(fig);
end

function stats = calculate_factor_statistics(i_data, q_data)
    channel = (0:7)';
    count = sum(isfinite(i_data) & isfinite(q_data), 1)';
    i_mean = mean(i_data, 1, 'omitnan')';
    i_variance = var(i_data, 0, 1, 'omitnan')';
    i_std = std(i_data, 0, 1, 'omitnan')';
    q_mean = mean(q_data, 1, 'omitnan')';
    q_variance = var(q_data, 0, 1, 'omitnan')';
    q_std = std(q_data, 0, 1, 'omitnan')';
    magnitude_data = hypot(i_data, q_data);
    magnitude_mean = mean(magnitude_data, 1, 'omitnan')';
    magnitude_variance = var(magnitude_data, 0, 1, 'omitnan')';
    magnitude_std = std(magnitude_data, 0, 1, 'omitnan')';
    stats = table(channel, count, i_mean, i_variance, i_std, q_mean, ...
        q_variance, q_std, magnitude_mean, magnitude_variance, magnitude_std);
end

function plot_all_scatter(factors, filename)
    fig = figure('Visible', 'on', 'Name', 'All ACM calibration I/Q scatter', 'Position', [100 100 1100 850]);
    colors = lines(8);
    hold on;
    for c = 0:7
        scatter(factors.i(:, c + 1), factors.q(:, c + 1), ...
            18, colors(c + 1, :), 'filled', 'DisplayName', sprintf('Channel %d', c));
    end
    axis equal; grid on; xlabel('I factor'); ylabel('Q factor');
    title(sprintf('All ACM calibration factors (%d results)', size(factors.i, 1)));
    legend('Location', 'bestoutside');
    saveas(fig, filename);
end

function plot_variance_magnitude(stats, filename)
    fig = figure('Visible', 'on', 'Name', 'ACM variance and magnitude mean', 'Position', [100 100 1200 500]);
    x = stats.channel;

    subplot(1, 2, 1);
    bar(x, [stats.i_variance stats.q_variance], 'grouped');
    grid on; xlabel('Channel'); ylabel('Variance');
    title('I/Q factor variance (all results)');
    legend({'I variance', 'Q variance'}, 'Location', 'best');
    set(gca, 'XTick', 0:7);

    subplot(1, 2, 2);
    bar(x, stats.magnitude_mean, 0.6);
    grid on; xlabel('Channel'); ylabel('Mean magnitude');
    title('Mean factor magnitude sqrt(I^2+Q^2) (all results)');
    set(gca, 'XTick', 0:7);
    for row = 1:height(stats)
        text(x(row), stats.magnitude_mean(row), sprintf(' %.4f', stats.magnitude_mean(row)), ...
            'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom', 'FontSize', 8);
    end
    saveas(fig, filename);
end

function print_summary(results, stats, startup, log_file, output_dir)
    complete = isfinite(results.generation);
    valid = complete & results.valid == 1 & results.publish_valid == 1;
    incomplete = ~complete;
    fprintf('\n=== ACM calibration log analysis ===\n');
    fprintf('Log: %s\n', log_file);
    if isfinite(startup.target)
        fprintf('Startup batch: target=%d, valid=%d, success rate=%.2f%%\n', ...
            startup.target, startup.valid, 100 * startup.valid / startup.target);
    end
    fprintf('Periodic attempts: %d (complete=%d, incomplete=%d)\n', ...
        height(results), sum(complete), sum(incomplete));
    fprintf('Periodic valid: %d/%d (%.2f%%)\n', sum(valid), sum(complete), 100 * sum(valid) / max(sum(complete), 1));
    if any(valid)
        fprintf('Elapsed: mean=%.1f us, std=%.1f us, range=[%.0f, %.0f] us\n', ...
            mean(results.elapsed_us(valid)), std(results.elapsed_us(valid)), min(results.elapsed_us(valid)), max(results.elapsed_us(valid)));
        fprintf('Trigger late: max=%.0f us; S0 period: mean=%.1f us, range=[%.0f, %.0f] us\n', ...
            max(results.trigger_late_us(valid)), mean(results.s0_period_after_us(valid)), ...
            min(results.s0_period_after_us(valid)), max(results.s0_period_after_us(valid)));
    end
    fprintf('\nPer-channel factor stability (valid periodic results):\n');
    fprintf('Channel   I mean/std        Q mean/std        Magnitude mean/std\n');
    for row = 1:height(stats)
        fprintf('%3d      % .5f/%-8.5f  % .5f/%-8.5f  %.5f/%.5f\n', ...
            stats.channel(row), stats.i_mean(row), stats.i_std(row), ...
            stats.q_mean(row), stats.q_std(row), stats.magnitude_mean(row), stats.magnitude_std(row));
    end
    fprintf('\nFiles written to: %s\n', output_dir);
end
