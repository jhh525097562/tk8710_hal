#!/usr/bin/env python3
"""Analyze 470-510 MHz RK3506 ACM startup-calibration sweep results."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


RUN_RE = re.compile(r"(?:sweep|rerun|round\d+)_(\d+)MHz")
VALID_RE = re.compile(r"ACM校准完成，有效校准次数:\s*(\d+)")
FACTOR_RE = re.compile(
    r"\[(\d+)\].*I_factor=0x([0-9A-Fa-f]+),\s*Q_factor=0x([0-9A-Fa-f]+)"
)


def signed18_fixed15(hex_text: str) -> float:
    value = int(hex_text, 16)
    if value >= (1 << 17):
        value -= 1 << 18
    return value / float(1 << 15)


def parse_factor_file(path: Path) -> tuple[np.ndarray, int]:
    if not path.exists():
        return np.empty((0, 8, 2), dtype=float), 0

    complete: list[np.ndarray] = []
    incomplete = 0
    current: dict[int, tuple[float, float]] = {}

    def finish_block() -> None:
        nonlocal incomplete, current
        if not current:
            return
        if set(current) == set(range(8)):
            block = np.array([current[channel] for channel in range(8)], dtype=float)
            complete.append(block)
        else:
            incomplete += 1
        current = {}

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.strip().startswith("==="):
            finish_block()
            continue
        match = FACTOR_RE.search(line)
        if match:
            channel = int(match.group(1))
            current[channel] = (
                signed18_fixed15(match.group(2)),
                signed18_fixed15(match.group(3)),
            )
    finish_block()

    if not complete:
        return np.empty((0, 8, 2), dtype=float), incomplete
    return np.stack(complete), incomplete


def sample_variance(values: np.ndarray) -> float:
    if values.size <= 1:
        return 0.0 if values.size == 1 else math.nan
    return float(np.var(values, ddof=1))


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def analyze_run(run_dir: Path, frequency_mhz: int) -> tuple[dict, list[dict], np.ndarray]:
    stdout_path = run_dir / "stdout_pty.log"
    stdout_text = stdout_path.read_text(encoding="utf-8", errors="replace")
    valid_matches = VALID_RE.findall(stdout_text)
    valid_count = int(valid_matches[-1]) if valid_matches else -1
    error_lines = [line for line in stdout_text.splitlines() if "[ERROR]" in line]
    warning_lines = [line for line in stdout_text.splitlines() if "[WARN]" in line]

    factors, incomplete_blocks = parse_factor_file(run_dir / "CaliFactor" / "CaliFactor.txt")
    block_count = int(factors.shape[0])
    channel_rows: list[dict] = []

    for channel in range(8):
        if block_count:
            i_values = factors[:, channel, 0]
            q_values = factors[:, channel, 1]
            magnitudes = np.hypot(i_values, q_values)
            i_mean = float(np.mean(i_values))
            q_mean = float(np.mean(q_values))
            i_variance = sample_variance(i_values)
            q_variance = sample_variance(q_values)
            magnitude_mean = float(np.mean(magnitudes))
            magnitude_variance = sample_variance(magnitudes)
        else:
            i_mean = q_mean = i_variance = q_variance = math.nan
            magnitude_mean = magnitude_variance = math.nan
        channel_rows.append(
            {
                "frequency_mhz": frequency_mhz,
                "channel": channel,
                "sample_count": block_count,
                "i_mean": i_mean,
                "q_mean": q_mean,
                "i_variance": i_variance,
                "q_variance": q_variance,
                "complex_variance": i_variance + q_variance,
                "magnitude_mean": magnitude_mean,
                "magnitude_variance": magnitude_variance,
            }
        )

    if block_count:
        all_i = factors[:, :, 0].reshape(-1)
        all_q = factors[:, :, 1].reshape(-1)
        all_magnitude = np.hypot(all_i, all_q)
        active_rows = channel_rows[1:]
        max_row = max(active_rows, key=lambda row: row["complex_variance"])
        summary = {
            "frequency_mhz": frequency_mhz,
            "valid_count": valid_count,
            "factor_blocks": block_count,
            "incomplete_factor_blocks": incomplete_blocks,
            "valid_matches_factor_blocks": valid_count == block_count,
            "target_reached": valid_count == 100 and block_count == 100,
            "error_count": len(error_lines),
            "warning_count": len(warning_lines),
            "overall_i_mean": float(np.mean(all_i)),
            "overall_q_mean": float(np.mean(all_q)),
            "overall_i_variance": sample_variance(all_i),
            "overall_q_variance": sample_variance(all_q),
            "overall_magnitude_mean": float(np.mean(all_magnitude)),
            "overall_magnitude_variance": sample_variance(all_magnitude),
            "avg_complex_variance_ch1_7": float(
                np.mean([row["complex_variance"] for row in active_rows])
            ),
            "max_complex_variance_ch1_7": max_row["complex_variance"],
            "max_variance_channel": max_row["channel"],
            "error_text": " || ".join(error_lines),
            "local_run_dir": str(run_dir),
        }
    else:
        summary = {
            "frequency_mhz": frequency_mhz,
            "valid_count": valid_count,
            "factor_blocks": 0,
            "incomplete_factor_blocks": incomplete_blocks,
            "valid_matches_factor_blocks": valid_count == 0,
            "target_reached": False,
            "error_count": len(error_lines),
            "warning_count": len(warning_lines),
            "overall_i_mean": math.nan,
            "overall_q_mean": math.nan,
            "overall_i_variance": math.nan,
            "overall_q_variance": math.nan,
            "overall_magnitude_mean": math.nan,
            "overall_magnitude_variance": math.nan,
            "avg_complex_variance_ch1_7": math.nan,
            "max_complex_variance_ch1_7": math.nan,
            "max_variance_channel": "",
            "error_text": " || ".join(error_lines),
            "local_run_dir": str(run_dir),
        }
    return summary, channel_rows, factors


def plot_results(output_dir: Path, summaries: list[dict], channel_rows: list[dict]) -> None:
    frequencies = np.array([row["frequency_mhz"] for row in summaries])
    valid_counts = np.array([row["valid_count"] for row in summaries])
    avg_variance = np.array([row["avg_complex_variance_ch1_7"] for row in summaries])

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
    axes[0].plot(frequencies, valid_counts, "o-", linewidth=1.5)
    axes[0].axhline(100, color="green", linestyle="--", linewidth=1, label="target=100")
    axes[0].set_ylabel("Valid calibration count")
    axes[0].set_ylim(-5, 105)
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()
    axes[1].semilogy(frequencies, np.maximum(avg_variance, 1e-12), "o-", linewidth=1.5)
    axes[1].set_xlabel("Frequency (MHz)")
    axes[1].set_ylabel("Mean Var(I)+Var(Q), CH1-CH7")
    axes[1].grid(True, which="both", alpha=0.3)
    fig.suptitle("TK8710 ACM startup calibration frequency sweep")
    fig.tight_layout()
    fig.savefig(output_dir / "acm_valid_count_and_variance.png", dpi=180)
    plt.close(fig)

    magnitude_matrix = np.full((8, len(frequencies)), np.nan)
    variance_matrix = np.full((8, len(frequencies)), np.nan)
    frequency_to_col = {frequency: index for index, frequency in enumerate(frequencies)}
    for row in channel_rows:
        col = frequency_to_col[row["frequency_mhz"]]
        magnitude_matrix[row["channel"], col] = row["magnitude_mean"]
        variance_matrix[row["channel"], col] = row["complex_variance"]

    fig, axes = plt.subplots(2, 1, figsize=(15, 8), sharex=True)
    image0 = axes[0].imshow(magnitude_matrix, aspect="auto", origin="lower", interpolation="nearest")
    axes[0].set_ylabel("Channel")
    axes[0].set_title("Mean factor magnitude")
    fig.colorbar(image0, ax=axes[0], label="mean |I+jQ|")
    image1 = axes[1].imshow(
        np.log10(np.maximum(variance_matrix, 1e-12)),
        aspect="auto",
        origin="lower",
        interpolation="nearest",
    )
    axes[1].set_ylabel("Channel")
    axes[1].set_xlabel("Frequency (MHz)")
    axes[1].set_title("log10(Var(I)+Var(Q))")
    tick_columns = np.arange(0, len(frequencies), 2)
    axes[1].set_xticks(tick_columns, frequencies[tick_columns])
    fig.colorbar(image1, ax=axes[1], label="log10 complex variance")
    fig.tight_layout()
    fig.savefig(output_dir / "acm_channel_mean_variance_heatmap.png", dpi=180)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs_root", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--start-mhz", type=int, default=470)
    parser.add_argument("--stop-mhz", type=int, default=510)
    args = parser.parse_args()

    if args.start_mhz > args.stop_mhz:
        parser.error("--start-mhz must not be greater than --stop-mhz")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    discovered: dict[int, Path] = {}
    for run_dir in args.runs_root.iterdir():
        if not run_dir.is_dir():
            continue
        match = RUN_RE.search(run_dir.name)
        if match:
            discovered[int(match.group(1))] = run_dir

    expected = set(range(args.start_mhz, args.stop_mhz + 1))
    missing = sorted(expected - set(discovered))
    if missing:
        raise SystemExit(f"Missing frequency runs: {missing}")

    summaries: list[dict] = []
    all_channel_rows: list[dict] = []
    for frequency_mhz in sorted(expected):
        summary, channel_rows, _ = analyze_run(discovered[frequency_mhz], frequency_mhz)
        summaries.append(summary)
        all_channel_rows.extend(channel_rows)

    write_csv(args.output_dir / "frequency_summary.csv", summaries)
    write_csv(args.output_dir / "channel_statistics.csv", all_channel_rows)
    plot_results(args.output_dir, summaries, all_channel_rows)

    compact = {
        "frequency_count": len(summaries),
        "target_reached_frequency_count": sum(row["target_reached"] for row in summaries),
        "valid_factor_consistency": all(row["valid_matches_factor_blocks"] for row in summaries),
        "total_valid_calibrations": sum(row["valid_count"] for row in summaries),
        "failed_frequencies_mhz": [
            row["frequency_mhz"] for row in summaries if not row["target_reached"]
        ],
        "frequency_with_max_avg_complex_variance": max(
            (row for row in summaries if not math.isnan(row["avg_complex_variance_ch1_7"])),
            key=lambda row: row["avg_complex_variance_ch1_7"],
        )["frequency_mhz"],
    }
    (args.output_dir / "analysis_summary.json").write_text(
        json.dumps(compact, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(compact, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
