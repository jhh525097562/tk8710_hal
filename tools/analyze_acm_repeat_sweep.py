#!/usr/bin/env python3
"""Analyze repeated RK3506 ACM startup-calibration frequency sweeps."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import statistics
from pathlib import Path

import matplotlib.pyplot as plt

from analyze_acm_frequency_sweep import analyze_run


RUN_RE = re.compile(r"repeat\d+_(\d+)MHz_r(\d+)")


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def mean_finite(values: list[float]) -> float:
    finite = [value for value in values if not math.isnan(value)]
    return statistics.mean(finite) if finite else math.nan


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs_root", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--start-mhz", type=int, required=True)
    parser.add_argument("--stop-mhz", type=int, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    discovered: dict[tuple[int, int], Path] = {}
    for run_dir in args.runs_root.iterdir():
        if not run_dir.is_dir():
            continue
        match = RUN_RE.search(run_dir.name)
        if match:
            discovered[(int(match.group(1)), int(match.group(2)))] = run_dir

    expected = {
        (frequency, repeat)
        for frequency in range(args.start_mhz, args.stop_mhz + 1)
        for repeat in range(1, args.repeats + 1)
    }
    missing = sorted(expected - set(discovered))
    if missing:
        raise SystemExit(f"Missing runs: {missing}")

    run_rows: list[dict] = []
    channel_rows: list[dict] = []
    for frequency, repeat in sorted(expected):
        run_dir = discovered[(frequency, repeat)]
        summary, channels, _ = analyze_run(run_dir, frequency)
        skill_summary = json.loads((run_dir / "summary.json").read_text(encoding="utf-8"))
        run_rows.append(
            {
                "frequency_mhz": frequency,
                "repeat": repeat,
                "valid_count": summary["valid_count"],
                "factor_blocks": summary["factor_blocks"],
                "target_reached": summary["target_reached"],
                "error_count": summary["error_count"],
                "warning_count": summary["warning_count"],
                "overall_i_mean": summary["overall_i_mean"],
                "overall_q_mean": summary["overall_q_mean"],
                "overall_magnitude_mean": summary["overall_magnitude_mean"],
                "avg_complex_variance_ch1_7": summary["avg_complex_variance_ch1_7"],
                "max_variance_channel": summary["max_variance_channel"],
                "exit_status": skill_summary["exit_status"],
                "forced_exit": skill_summary["forced_exit"],
                "remote_run_dir": skill_summary["remote_run_dir"],
                "local_run_dir": str(run_dir),
            }
        )
        for row in channels:
            row = dict(row)
            row["repeat"] = repeat
            channel_rows.append(row)

    frequency_rows: list[dict] = []
    for frequency in range(args.start_mhz, args.stop_mhz + 1):
        rows = [row for row in run_rows if row["frequency_mhz"] == frequency]
        counts = [int(row["valid_count"]) for row in rows]
        variance_values = [float(row["avg_complex_variance_ch1_7"]) for row in rows]
        magnitude_values = [float(row["overall_magnitude_mean"]) for row in rows]
        frequency_rows.append(
            {
                "frequency_mhz": frequency,
                **{f"repeat_{index + 1}_valid": value for index, value in enumerate(counts)},
                "valid_mean": statistics.mean(counts),
                "valid_sample_std": statistics.stdev(counts) if len(counts) > 1 else 0.0,
                "valid_min": min(counts),
                "valid_max": max(counts),
                "valid_range": max(counts) - min(counts),
                "target_pass_runs": sum(value == 100 for value in counts),
                "target_pass_rate": sum(value == 100 for value in counts) / len(counts),
                "mean_factor_magnitude": mean_finite(magnitude_values),
                "mean_complex_variance_ch1_7": mean_finite(variance_values),
            }
        )

    write_csv(args.output_dir / "repeat_run_statistics.csv", run_rows)
    write_csv(args.output_dir / "repeat_frequency_summary.csv", frequency_rows)
    write_csv(args.output_dir / "repeat_channel_statistics.csv", channel_rows)

    frequencies = [row["frequency_mhz"] for row in frequency_rows]
    fig, axes = plt.subplots(2, 1, figsize=(12, 9), sharex=True)
    for repeat in range(1, args.repeats + 1):
        axes[0].plot(
            frequencies,
            [row[f"repeat_{repeat}_valid"] for row in frequency_rows],
            marker="o",
            label=f"repeat {repeat}",
        )
    axes[0].axhline(100, color="green", linestyle="--", linewidth=1, label="target=100")
    axes[0].set_ylabel("Valid calibration count")
    axes[0].set_ylim(-5, 105)
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()
    axes[1].errorbar(
        frequencies,
        [row["valid_mean"] for row in frequency_rows],
        yerr=[row["valid_sample_std"] for row in frequency_rows],
        marker="o",
        capsize=4,
    )
    axes[1].set_xlabel("Frequency (MHz)")
    axes[1].set_ylabel("Mean valid count +/- sample std")
    axes[1].set_xticks(frequencies)
    axes[1].set_ylim(-5, 105)
    axes[1].grid(True, alpha=0.3)
    fig.suptitle("ACM startup calibration: repeated frequency sweep")
    fig.tight_layout()
    fig.savefig(args.output_dir / "repeat_valid_count_statistics.png", dpi=180)
    plt.close(fig)

    # Keep the same two-panel definition as the single-sweep
    # acm_valid_count_and_variance.png, while retaining all repeated runs.
    variance_by_repeat = {
        repeat: [
            float(
                next(
                    row["avg_complex_variance_ch1_7"]
                    for row in run_rows
                    if row["frequency_mhz"] == frequency and row["repeat"] == repeat
                )
            )
            for frequency in frequencies
        ]
        for repeat in range(1, args.repeats + 1)
    }
    valid_by_repeat = {
        repeat: [row[f"repeat_{repeat}_valid"] for row in frequency_rows]
        for repeat in range(1, args.repeats + 1)
    }
    mean_variance = [
        mean_finite([variance_by_repeat[repeat][index] for repeat in variance_by_repeat])
        for index in range(len(frequencies))
    ]
    min_variance = [
        min(variance_by_repeat[repeat][index] for repeat in variance_by_repeat)
        for index in range(len(frequencies))
    ]
    max_variance = [
        max(variance_by_repeat[repeat][index] for repeat in variance_by_repeat)
        for index in range(len(frequencies))
    ]

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
    for repeat in range(1, args.repeats + 1):
        axes[0].plot(
            frequencies,
            valid_by_repeat[repeat],
            "o-",
            linewidth=1.1,
            markersize=4,
            alpha=0.75,
            label=f"repeat {repeat}",
        )
    axes[0].plot(
        frequencies,
        [row["valid_mean"] for row in frequency_rows],
        "k-",
        linewidth=2.2,
        label="3-run mean",
    )
    axes[0].axhline(100, color="green", linestyle="--", linewidth=1, label="target=100")
    axes[0].set_ylabel("Valid calibration count")
    axes[0].set_ylim(-5, 105)
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(ncol=5, fontsize=8)

    for repeat in range(1, args.repeats + 1):
        axes[1].semilogy(
            frequencies,
            [max(value, 1e-12) for value in variance_by_repeat[repeat]],
            "o-",
            linewidth=1.0,
            markersize=3.5,
            alpha=0.65,
            label=f"repeat {repeat}",
        )
    axes[1].fill_between(
        frequencies,
        [max(value, 1e-12) for value in min_variance],
        [max(value, 1e-12) for value in max_variance],
        color="gray",
        alpha=0.18,
        label="3-run min-max",
    )
    axes[1].semilogy(
        frequencies,
        [max(value, 1e-12) for value in mean_variance],
        "k-",
        linewidth=2.2,
        label="3-run mean",
    )
    axes[1].set_xlabel("Frequency (MHz)")
    axes[1].set_ylabel("Mean Var(I)+Var(Q), CH1-CH7")
    positive_variances = [
        value
        for values in variance_by_repeat.values()
        for value in values
        if math.isfinite(value) and value > 0
    ]
    if positive_variances:
        variance_lower = 10 ** math.floor(math.log10(min(positive_variances)))
        variance_upper = 10 ** math.ceil(math.log10(max(positive_variances)))
        if variance_lower == variance_upper:
            variance_upper *= 10
        axes[1].set_ylim(variance_lower, variance_upper)
    axes[1].set_xticks(frequencies[::2])
    axes[1].grid(True, which="both", alpha=0.3)
    axes[1].legend(ncol=5, fontsize=8)
    fig.suptitle("TK8710 ACM startup calibration frequency sweep (3 runs/frequency)")
    fig.tight_layout()
    fig.savefig(args.output_dir / "acm_valid_count_and_variance.png", dpi=180)
    plt.close(fig)

    compact = {
        "frequency_count": len(frequency_rows),
        "repeat_count_per_frequency": args.repeats,
        "run_count": len(run_rows),
        "normal_exit_count": sum(
            row["exit_status"] == 0 and not row["forced_exit"] for row in run_rows
        ),
        "total_valid_calibrations": sum(int(row["valid_count"]) for row in run_rows),
        "target_reached_run_count": sum(bool(row["target_reached"]) for row in run_rows),
        "all_three_pass_frequencies_mhz": [
            row["frequency_mhz"] for row in frequency_rows if row["target_pass_runs"] == args.repeats
        ],
        "no_pass_frequencies_mhz": [
            row["frequency_mhz"] for row in frequency_rows if row["target_pass_runs"] == 0
        ],
    }
    (args.output_dir / "repeat_analysis_summary.json").write_text(
        json.dumps(compact, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(compact, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
