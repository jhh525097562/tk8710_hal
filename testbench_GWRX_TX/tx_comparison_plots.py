"""Automatic eight-antenna I/Q figures shared by comparison runners."""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def _read(path):
    v=np.loadtxt(path,ndmin=2)
    if v.shape[1]!=16 or not len(v) or not np.isfinite(v).all():
        raise ValueError(f'Expected finite eight-antenna I/Q data: {path}')
    return v[:,::2]+1j*v[:,1::2]


def _hash(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def generate_tx_plots(reference_file,actual_file,output_dir,*,mode,scale=2**23,sample_rate=125000,context=''):
    reference_file=Path(reference_file).resolve();actual_file=Path(actual_file).resolve()
    reference=_read(reference_file);actual=_read(actual_file)*scale
    if not np.isfinite(actual).all() or not np.isfinite(scale) or scale<=0:raise ValueError('Invalid scale')
    out=Path(output_dir).resolve()/('tx_plots_'+dt.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    out.mkdir(parents=True,exist_ok=False)
    n=min(len(reference),len(actual));error=actual[:n]-reference[:n]
    rms=np.sqrt(np.mean(abs(reference[:n])**2,axis=0))
    center=int(np.argmax(np.max(abs(error)/np.where(rms>0,rms,1.),axis=1)))
    lo=max(0,center-32);hi=min(n,center+33)
    if hi-lo<65:lo=max(0,hi-65);hi=min(n,lo+65)
    zoom=np.arange(lo,hi);nr=np.arange(len(reference));nc=np.arange(len(actual))
    label=f'C_model {mode} × {scale:g}'
    rows=[]
    for ant in range(8):
        norm=np.linalg.norm(reference[:n,ant]);diff=np.linalg.norm(error[:,ant])
        rows.append({'antenna':ant+1,'relative_l2_percent':float(diff/norm*100) if norm>0 else None,
            'complex_rmse':float(np.sqrt(np.mean(abs(error[:,ant])**2))),
            'max_absolute_error':float(np.max(abs(error[:,ant])))})
    plt.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Microsoft YaHei','DejaVu Sans'],
                         'axes.unicode_minus':False,'path.simplify':False,'font.size':10})
    def overlay(ax,ant,part,zoomed):
        x1,x2=(zoom,zoom) if zoomed else (nr,nc)
        ax.plot(x1,getattr(reference[:,ant],part)[x1],color='#2463ae',lw=1.1,label='MATLAB')
        ax.plot(x2,getattr(actual[:,ant],part)[x2],color='#e07b22',ls='--',lw=.9,label=label,
                marker='.' if zoomed else None,ms=3)
        ax.grid(alpha=.22);ax.set_ylabel(f'天线 {ant+1} / '+('I' if part=='real' else 'Q'))
        if zoomed:ax.set_xlim(zoom[0],max(zoom[-1],zoom[0]+1))
        else:ax.set_xlim(0,max(1,max(len(reference),len(actual))-1))
    header=f'8 天线 I/Q：{mode} | {sample_rate:g} sample/s | MATLAB {len(reference)} 点 / C_model {len(actual)} 点'
    for name,zoomed in (('all_antennas_zoom',True),('all_antennas_full',False)):
        fig,axes=plt.subplots(8,2,figsize=(16,19),sharex=True,layout='constrained')
        for ant in range(8):
            for col,part in enumerate(('real','imag')):overlay(axes[ant,col],ant,part,zoomed)
        for ax in axes[0]:ax.legend(fontsize=8)
        for ax in axes[-1]:ax.set_xlabel('采样索引（从 0 开始）')
        fig.suptitle(header+'\n'+(f'最大归一化误差附近窗口：{lo}–{hi-1}' if zoomed else '完整波形；密集区请看局部放大图')+' | '+context)
        fig.savefig(out/(name+'.png'),dpi=140);plt.close(fig)
    for ant in range(8):
        fig,axes=plt.subplots(3,2,figsize=(16,10),layout='constrained')
        for col in range(2):
            for row,part in enumerate(('real','imag')):overlay(axes[row,col],ant,part,col==1)
            axes[0,col].legend(fontsize=8)
            axes[0,col].set_title('完整波形' if col==0 else f'采样 {lo}–{hi-1} 局部放大')
        axes[2,0].plot(np.arange(n),abs(error[:,ant]),color='#b63838',lw=.7)
        axes[2,0].set_ylabel('|C−M| 绝对误差')
        axes[2,1].plot(zoom,abs(reference[zoom,ant]),label='|MATLAB|',color='#2463ae')
        axes[2,1].plot(zoom,abs(actual[zoom,ant]),label='|C_model|',color='#e07b22',ls='--')
        axes[2,1].plot(zoom,abs(error[zoom,ant]),label='|C−M|',color='#b63838')
        axes[2,1].set_ylabel('复数幅度 / 绝对误差');axes[2,1].legend(fontsize=8)
        for ax in axes[-1]:ax.set_xlabel('采样索引（从 0 开始）');ax.grid(alpha=.22)
        rel=rows[ant]['relative_l2_percent'];reltext=f'{rel:.4f}%' if rel is not None else 'N/A（参考能量为零）'
        fig.suptitle(f'天线 {ant+1} | {mode} | 相对 L2={reltext} | {context}\n只还原固定缩放，不拟合增益、相位或时延')
        fig.savefig(out/f'antenna_{ant+1:02d}.png',dpi=150);plt.close(fig)
    manifest={'mode':mode,'context':context,'reference_file':str(reference_file),'actual_file':str(actual_file),
        'reference_sha256':_hash(reference_file),'actual_sha256':_hash(actual_file),'plotter_sha256':_hash(__file__),
        'scale':scale,'sample_rate':sample_rate,'reference_shape':list(reference.shape),'actual_shape':list(actual.shape),
        'same_shape':reference.shape==actual.shape,'compared_samples':n,'zoom_range':[lo,hi-1],'metrics':rows}
    (out/'plots.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    report=['# MATLAB / C_model 8 天线 I/Q 对比图','',f'模式：**{mode}**；{context}。采样率：{sample_rate:g} sample/s。',
        '',f'蓝色实线：MATLAB；橙色虚线：C_model，已乘固定缩放系数 {scale}。',
        '','![8天线局部放大](all_antennas_zoom.png)','', '[8天线完整波形总图](all_antennas_full.png)', '',
        '## 各天线 I/Q 完整波形、局部放大与误差','']
    report += [f'- [天线 {ant}](antenna_{ant:02d}.png)' for ant in range(1,9)]
    report += ['',f'MATLAB 长度 {len(reference)}，C_model 长度 {len(actual)}；长度一致：{reference.shape==actual.shape}。',
        f'局部窗口 {lo}～{hi-1}，位于各天线按参考 RMS 归一后的最大误差附近。未进行波形对齐、重采样或增益/相位拟合。',
        '局部窗口用于检查最明显差异，不代表整段波形。下表按共同长度计算；若长度不同，不能据共同部分指标判为一致。', '',
        '| 天线 | 相对 L2 | 复数 RMSE | 最大绝对误差 |','|---|---:|---:|---:|']
    for row in rows:
        rel=row['relative_l2_percent'];txt=f'{rel:.4f}%' if rel is not None else 'N/A'
        report.append(f'| {row["antenna"]} | {txt} | {row["complex_rmse"]:.6g} | {row["max_absolute_error"]:.6g} |')
    report += ['', '误差按整段复数能量归一化，不除以瞬时 I/Q，避免过零点夸大百分比。图像不替代接收解调、RTL或硬件验收。',
        '', '[数据路径、哈希与绘图参数](plots.json)', '']
    index=out/'图像索引.md';index.write_text('\n'.join(report),encoding='utf-8')
    result={'directory':str(out),'index':str(index),'overview_zoom':str(out/'all_antennas_zoom.png'),
            'overview_full':str(out/'all_antennas_full.png'),'antenna_images':[str(out/f'antenna_{i:02d}.png') for i in range(1,9)]}
    print('TX_PLOTS='+str(index),flush=True)
    return result


if __name__=='__main__':
    sys.stdout.reconfigure(errors='backslashreplace')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--actual',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--mode',required=True)
    parser.add_argument('--scale',type=float,default=2**23)
    parser.add_argument('--sample-rate',type=float,default=125000)
    parser.add_argument('--context',default='')
    args=parser.parse_args()
    generate_tx_plots(args.reference,args.actual,args.output,mode=args.mode,scale=args.scale,
                      sample_rate=args.sample_rate,context=args.context)
