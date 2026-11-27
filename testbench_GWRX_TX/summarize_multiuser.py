"""Create human-readable artifacts from a completed multiuser comparison."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from run_matlab_compare import load_iq
from run_smoke import digest


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('run',type=Path)
    p=ap.parse_args().run.resolve()
    result=json.loads((p/'comparison.json').read_text(encoding='utf-8'))
    if result['status'] not in ('exact_match','completed_with_differences'):
        raise ValueError('Comparison did not complete')
    rx=result['rx']; total=result['generated_users']
    mdir=Path(result.get('reused_matlab_data',str(p/'matlab')))
    reference=load_iq(mdir/'matlab_tx_8ant_baseband.txt',8)
    actual=load_iq(p/'cmodel/GWTXModule/case95/tx_frame0.txt',8)*result['tx_canonical']['cmodel_scale']
    assert digest(mdir/'matlab_tx_8ant_baseband.txt')==result['waveform_sha256']['matlab_baseband']
    assert reference.shape==actual.shape
    plt.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Microsoft YaHei','DejaVu Sans'],
                         'axes.unicode_minus':False,'path.simplify':False})
    error=actual-reference
    center=int(np.argmax(np.sum(abs(error)**2,axis=1)))
    lo,hi=max(0,center-35),min(len(reference),center+36)
    n=np.arange(lo,hi)
    fig,axes=plt.subplots(8,2,figsize=(16,18),sharex=True,layout='constrained')
    for ant in range(8):
        for col,part in enumerate(('real','imag')):
            ax=axes[ant,col]
            ax.plot(n,getattr(reference[n,ant],part),label='MATLAB',color='#2463ae',lw=1)
            ax.plot(n,getattr(actual[n,ant],part),label='C_model float × 2²³',color='#e07b22',ls='--',lw=1)
            ax.set_ylabel(f'天线 {ant+1} / '+('I' if col==0 else 'Q'));ax.grid(alpha=.25)
    for ax in axes[0]:ax.legend(fontsize=8)
    for ax in axes[-1]:ax.set_xlabel('采样索引（从 0 开始）')
    fig.suptitle(f'{total} 用户输入，MATLAB RX {rx["matlab_users"]}，C_model RX {rx["cmodel_users"]}\n各自 RX 驱动 TX，最大总误差附近窗口 {lo}–{hi-1}；未进行增益/时延拟合')
    fig.savefig(p/'tx_8ant_zoom.png',dpi=140);plt.close(fig)
    rows=[]
    details={d['user_id']:d for d in rx['details']}
    for uid in range(1,total+1):
        d=details.get(uid,{})
        rows.append({'user_id':uid,'matlab_received':uid not in rx['missing_matlab'],
            'cmodel_received':uid not in rx['missing_cmodel'],'payload_equal':d.get('payload_equal',''),
            'matlab_payload_matches_transmitted':uid not in rx['matlab_payload_errors'] if uid not in rx['missing_matlab'] else '',
            'cmodel_payload_matches_transmitted':uid not in rx['cmodel_payload_errors'] if uid not in rx['missing_cmodel'] else '',
            'frequency_error_hz':d.get('frequency_error_hz',''),'ah_relative_l2':d.get('ah_relative_l2',''),
            'pilot_power_error':d.get('pilot_power_error','')})
    with (p/'rx_128user_details.csv').open('w',encoding='utf-8-sig',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    lines=[f'# {total} 用户单帧 MATLAB / C_model float 对比', '',
        f'本次实际生成 {total} 个终端叠加的一帧数据，采用既有 case95 派生的 RX mode4 / TX mode14、8 天线、30 dB 配置、种子 1。关闭额外 CFO、漂移、时偏和干扰，随机用户频率/信道与原模型一致。', '',
        '沿用现有 float（double）分支：TX 内部 SVD、RX Viterbi 状态仍含 fxp；这不是全链路纯 double。已知相位回绕问题未修复，未更改 FFT/SVD/BF/调制算法。', '',
        '## RX 实测', '', '| 项目 | 结果 |','|---|---|',
        f'| 实际发送用户 | {total} |',f'| MATLAB 解调用户 | {rx["matlab_users"]}/{total} |',
        f'| C_model 解调用户 | {rx["cmodel_users"]}/{total} |',
        f'| 共同用户 | {rx["common_users"]} |',
        f'| 共同用户载荷逐字节相同 | {rx["payload_equal_common_users"]}/{rx["common_users"]} |',
        f'| MATLAB 对发送原文载荷错误 | {rx["matlab_payload_errors"]} |',
        f'| C_model 对发送原文载荷错误 | {rx["cmodel_payload_errors"]} |',
        f'| 用户集合与载荷均一致 | {rx["same_user_set_and_payload"]} |', '',
        f'仅 MATLAB 收到：`{rx["only_matlab"]}`。',f'仅 C_model 收到：`{rx["only_cmodel"]}`。',
        f'MATLAB 未收到：`{rx["missing_matlab"]}`。',f'C_model 未收到：`{rx["missing_cmodel"]}`。','',
        '以当前数据生成器的用户编码 `byte0*255+byte1` 匹配，不按检测行序比较；每个用户比较 26 字节载荷，C++ 尾部 2 个 PHY 字节排除。此编码仅用于本模型测试向量，不等同于外部终端协议。', '',
        '## TX 实测', '',
        '各模型给自身成功解调的用户发送下行；TX 载荷由 128 用户 ID 索引的 bit bank 提供，每个用户 46 字节确定性载荷，不因检测顺序改变。C++ 日志中的全部 ID→bit 行映射已核对。', '',
        '**若两边接收用户集合不同，以下总波形误差同时包含用户集合/平均功率分配差异，不能单独用来量化 TX 算法误差。没有丢弃不共同的用户来制造一致结论。**', '',
        f'基带形状：MATLAB `{result["tx_canonical"]["reference_shape"]}`，C++ `{result["tx_canonical"]["actual_shape"]}`（采样 × 天线）；125 ksample/s。MATLAB 1 Msample/s 的 8 天线最终波形也已保存。',
        'C++ 仅还原固定 2^23 缩放；不拟合增益、相位或延迟。', '',
        '| 天线 | 相对 L2 误差 | 复数 RMSE | 最大绝对误差 |','|---|---:|---:|---:|']
    for d in result['tx_canonical']['per_antenna']:
        lines.append(f'| {d["antenna"]} | {d["relative_l2_error"]*100:.4f}% | {d["complex_rmse"]:.6g} | {d["max_complex_absolute_error"]:.6g} |')
    lines += ['',f'精确相等：`{result["tx_canonical"]["exact_equal"]}`；流程状态：`{result["status"]}`。', '',
        '![8 天线 I/Q 局部叠加](tx_8ant_zoom.png)', '', '## 复现与证据', '',
        '```powershell',r"Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'",
        'python .\testbench_GWRX_TX\run_multiuser_compare.py --users 128','```','',
        '退出 1 表示完整比较发现差异，不代表生成/程序运行失败。退出 2 才表示流程未完成。单帧观察不能推断长期丢包率；程序旧 LostRate 的 mode4 分母仍为 96，本报告不采用它，统计以 128 个发送 ID 为准。', '',
        '- [全量比较 JSON](comparison.json)', '- [128 用户逐行结果 CSV](rx_128user_details.csv)',
        '- [MATLAB 日志](matlab.log)', '- [C++ 构建日志](build.log)', '- [C++ RX/TX 日志](cmodel.log)',
        '- [C++ RX 结果](cmodel/K2201DemodModule_Data/case95/rx_frame0.txt)',
        '- [C++ 8 天线 TX](cmodel/GWTXModule/case95/tx_frame0.txt)',
        f'- MATLAB 数据目录：`{mdir}`。含 `matlab_tx_8ant_baseband.txt`、`matlab_tx_8ant_rf.txt`、`matlab_one_frame.mat`、`matlab_results.json`、`tx_bits.txt`。','',
        f'C++ 执行耗时：{result["cmodel_seconds"]:.3f} 秒；MATLAB 耗时：{result.get("matlab_seconds","本次复用")}。仅作记录，不作为性能验收。','',
        '修改仅涉及多用户数据生成/匹配/载荷映射和结果输出；MATLAB 原工程由只读 hooks 调用。未修复已知相位问题或其他模型差异，未进行 RTL/硬件验证。']
    (p/'128用户收发对比报告.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('REPORT='+str(p/'128用户收发对比报告.md'))


if __name__=='__main__':main()
