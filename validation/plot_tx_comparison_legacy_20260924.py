"""Plot saved MATLAB/C-model I/Q without altering algorithms or source data."""
import argparse
import csv
import datetime as dt
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from run_matlab_compare import load_iq
from run_smoke import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    project = Path(__file__).resolve().parents[1]
    parser.add_argument("--run", type=Path, default=project / "validation/matlab_compare_float_20260923_180530_159111")
    args = parser.parse_args()
    run = args.run.resolve()
    evidence = json.loads((run / "comparison.json").read_text(encoding="utf-8"))
    reference_dir = Path(evidence.get("reused_matlab_data", str(run / "matlab")))
    ref_file = reference_dir / "matlab_tx_8ant_baseband.txt"
    actual_file = run / "cmodel/GWTXModule/case95/tx_frame0.txt"
    for path, key in ((ref_file, "matlab_baseband"), (actual_file, "cmodel")):
        if digest(path) != evidence["waveform_sha256"][key]:
            raise ValueError(f"Waveform changed since recorded comparison: {path}")
    reference = load_iq(ref_file, 8)
    actual = load_iq(actual_file, 8) * evidence["matlab_metadata"]["cmodel_tx_scale"]
    if reference.shape != actual.shape:
        raise ValueError("Unequal waveform shapes: refusing to crop")
    out = run / ("tx_plots_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    out.mkdir(exist_ok=False)
    plt.rcParams.update({"font.family": "sans-serif", "font.sans-serif": ["Microsoft YaHei", "DejaVu Sans"],
                         "axes.unicode_minus": False, "font.size": 10, "path.simplify": False})
    n = np.arange(len(reference))
    fs = evidence["matlab_metadata"]["tx_baseband_rate"]
    error = actual - reference
    amp = np.abs(reference)
    rms = np.sqrt(np.mean(amp**2, axis=0))
    normalized_error = np.abs(error) / rms
    large = normalized_error > .01  # Diagnostic partition, NOT acceptance threshold.
    first = int(np.flatnonzero(np.any(large, axis=1))[0]) if large.any() else int(np.argmax(normalized_error.max(axis=1)))
    center = int(np.argmax(normalized_error.max(axis=1)))
    lo, hi = max(0, center - 24), min(len(n), center + 25)
    rows = []
    for ant in range(8):
        mask = large[:, ant]
        energy = np.abs(error[:, ant])**2
        phase = np.angle(actual[:, ant] * np.conj(reference[:, ant]), deg=True)
        row = {"antenna": ant+1, "samples": len(n), "reference_complex_rms": float(rms[ant]),
               "relative_l2_percent": float(np.linalg.norm(error[:, ant])/np.linalg.norm(reference[:, ant])*100),
               "error_energy_percent_of_reference": float(np.sum(energy)/np.sum(amp[:, ant]**2)*100),
               "large_error_count": int(mask.sum()), "large_error_sample_percent": float(mask.mean()*100),
               "large_error_energy_share_percent": float(energy[mask].sum()/energy.sum()*100),
               "remaining_relative_l2_percent": float(np.linalg.norm(error[~mask, ant])/np.linalg.norm(reference[~mask, ant])*100),
               "min_complex_amplitude_over_rms": float(amp[:, ant].min()/rms[ant]),
               "max_complex_amplitude_over_rms": float(amp[:, ant].max()/rms[ant]),
               "near_zero_complex_samples": int(np.count_nonzero(amp[:, ant] < .01*rms[ant])),
               "max_abs_phase_error_deg": float(np.max(np.abs(phase)))}
        for component, ref, diff in (("I",reference[:,ant].real,error[:,ant].real),
                                     ("Q",reference[:,ant].imag,error[:,ant].imag)):
            near = np.abs(ref) < .01*rms[ant]
            row[f"near_zero_{component}_samples"] = int(near.sum())
            row[f"near_zero_{component}_error_energy_share_percent"] = float(np.sum(diff[near]**2)/energy.sum()*100)
        rows.append(row)
        fig, axes = plt.subplots(3,2,figsize=(16,10),layout="constrained")
        for col, (idx,title) in enumerate(((n,"完整一帧（密集波形需结合右侧放大图）"),(n[lo:hi],f"局部放大：采样 {lo}–{hi-1}"))):
            for ax, component in zip(axes[:2,col], ("real","imag")):
                ax.plot(idx,getattr(reference[:,ant],component)[idx],label="MATLAB",color="#2463ae",lw=1.3)
                ax.plot(idx,getattr(actual[:,ant],component)[idx],label="C_model float × 2²³",color="#e07b22",ls="--",lw=1,
                        marker="o" if col else None,ms=2.5)
                ax.set_ylabel("I 幅度" if component=="real" else "Q 幅度")
                ax.grid(alpha=.23); ax.set_xlim(idx[0],idx[-1]); ax.legend(loc="upper right",fontsize=8)
            axes[0,col].set_title(title)
        axes[2,0].plot(n,normalized_error[:,ant],color="#b63838",lw=.65,label="|C−M| / 整帧复数 RMS")
        axes[2,0].axhline(.01,color="gray",ls=":",label="1%：仅作诊断分组")
        axes[2,0].set_ylabel("复数绝对误差 / RMS")
        axes[2,0].legend(fontsize=8); axes[2,0].grid(alpha=.23)
        axes[2,1].plot(n[lo:hi],amp[lo:hi,ant],color="#2463ae",label="|MATLAB|")
        axes[2,1].plot(n[lo:hi],np.abs(actual[lo:hi,ant]),color="#e07b22",ls="--",label="|C_model|")
        axes[2,1].plot(n[lo:hi],np.abs(error[lo:hi,ant]),color="#b63838",label="|C−M|")
        axes[2,1].set_ylabel("复数幅度 / 绝对误差"); axes[2,1].grid(alpha=.23); axes[2,1].legend(fontsize=8)
        for ax in axes[-1]: ax.set_xlabel("采样索引（从 0 开始）")
        fig.suptitle(f"天线 {ant+1}：I/Q 叠加比较 | {fs:g} sample/s | L2={row['relative_l2_percent']:.4f}%\n"
                     f"大误差点 {mask.sum()}/{len(n)}；占误差能量 {row['large_error_energy_share_percent']:.4f}% | 无时移、相位或增益拟合")
        fig.savefig(out/f"antenna_{ant+1:02d}.png",dpi=150)
        plt.close(fig)
    # All eight antennas, I and Q on the same page, at identical sample indices.
    for tag, idx in (("all_antennas_zoom",n[lo:hi]),("all_antennas_full",n)):
        fig,axes=plt.subplots(8,2,figsize=(16,19),sharex=True,layout="constrained")
        for ant in range(8):
            for col, component in enumerate(("real","imag")):
                ax=axes[ant,col]
                ax.plot(idx,getattr(reference[:,ant],component)[idx],color="#2463ae",lw=1.2,label="MATLAB")
                ax.plot(idx,getattr(actual[:,ant],component)[idx],color="#e07b22",ls="--",lw=.9,label="C_model float × 2²³")
                ax.set_ylabel(f"天线 {ant+1} / {'I' if col==0 else 'Q'}")
                ax.grid(alpha=.23); ax.set_xlim(idx[0],idx[-1])
        for ax in axes[0]: ax.legend(fontsize=9)
        for ax in axes[-1]: ax.set_xlabel("采样索引（从 0 开始）")
        fig.suptitle("8 天线 I/Q 对比 — " + (f"共同局部窗口 {lo}–{hi-1}" if tag.endswith("zoom") else "完整一帧（另提供局部放大图）"))
        fig.savefig(out/f"{tag}.png",dpi=140); plt.close(fig)
    with (out/"metrics.csv").open("w",encoding="utf-8-sig",newline="") as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    (out/"metrics.json").write_text(json.dumps({"source_run":str(run),"source_hashes":evidence["waveform_sha256"],
        "script_sha256":digest(Path(__file__)),"scale":evidence["matlab_metadata"]["cmodel_tx_scale"],
        "first_large_sample":first,"zoom_range_inclusive":[lo,hi-1],"rows":rows},ensure_ascii=False,indent=2),encoding="utf-8")
    report=["# TX 八天线 I/Q 叠加图与近零点检查","", "使用 2026-09-23 float 实测输出；仅绘图和统计，未重跑或修改算法。",
            "", "## 图像入口", "", "![8 天线局部 I/Q 对比](all_antennas_zoom.png)","",
            "[完整一帧的 8 天线总图](all_antennas_full.png)","",
            "每根天线单独一张图，含 I、Q 完整波形、局部放大、复数幅度与误差：", ""]
    report += [f"- [天线 {a}](antenna_{a:02d}.png)" for a in range(1,9)]
    report += ["", "## 应怎样比较", "",
        "I、Q 各自叠加观察形状，误差用绝对值或整帧复数 RMS 归一化，避免用瞬时 I/Q 作为分母。主要指标为 `norm(C−M)/norm(M)`（整帧相对 L2 / RMS EVM）；复数包络为 `sqrt(I²+Q²)`。本图只还原 C++ 固定 2^23 缩放，不拟合增益/相位/延迟。",
        "", "I 或 Q 过零时，逐点相对误差可能虚高，这一点成立。但本帧 MATLAB 复数包络基本恒定：每路最小和最大包络/整帧 RMS 均约为 1，复数幅度低于 1% RMS 的点为 0。因此本次 15.54%～15.55% 的整帧 L2 误差不是除以近零参考值造成。",
        "", "## 误差分布", "", "1% RMS 仅用于划分诊断区域，不是验收门限。保留全部点作为主结论；排除大误差点的指标只是显示其余区间的相似程度。", "",
        "| 天线 | 整帧 L2/EVM | 大误差点数 | 所占误差能量 | 其余点 L2 | 峰值相位差 |",
        "|---|---:|---:|---:|---:|---:|"]
    report += [f"| {x['antenna']} | {x['relative_l2_percent']:.4f}% | {x['large_error_count']} | {x['large_error_energy_share_percent']:.4f}% | {x['remaining_relative_l2_percent']:.4f}% | {x['max_abs_phase_error_deg']:.3f}° |" for x in rows]
    report += ["",f"每路大误差点占 {rows[0]['large_error_sample_percent']:.4f}%，集中在共同的局部窗口。首个较大差异从索引 {first} 开始；放大图包含最严重的索引 {center}，可见幅度未消失但波形/相位明显偏离。整帧误差能量相对参考能量约 {min(x['error_energy_percent_of_reference'] for x in rows):.3f}%～{max(x['error_energy_percent_of_reference'] for x in rows):.3f}%。",
        "", "不能单凭这些点数量少就认定不影响接收；也不能直接据此认定会丢包。是否影响下行解调、频谱/EVM 要求，需要用同一接收模型解调两套下行波形，或按明确门限进一步验证。本次没有做该验证。",
        "", "[完整数值指标 CSV](metrics.csv) · [来源哈希及 JSON](metrics.json)", ""]
    (out/"图像与指标说明.md").write_text("\n".join(report),encoding="utf-8")
    print("OUTPUT="+str(out))
    print(json.dumps(rows,ensure_ascii=False,indent=2))


if __name__ == "__main__":
    main()
