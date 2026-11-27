"""Validate adaptive SVD while preserving the existing 22-bit square root."""
import datetime as dt
import argparse
import json
from pathlib import Path
import re
import sys
import numpy as np
from run_matlab_compare import load_iq,compare_iq,run_logged,save_json
from run_imported_tx_compare import check_imports
from run_smoke import digest
from tx_comparison_plots import generate_tx_plots


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--numeric-mode',choices=('fixed','float'),default='fixed')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=root/'validation'/('adaptive_svd_'+args.numeric_mode+'_6_'+dt.datetime.now().strftime('%Y%m%d_%H%M%S'))
    out.mkdir()
    compiler='C:/Program Files/mingw64/bin/g++.exe'
    trace=root/'validation/tx_svd_precision_20260924_153646/svd_double/output/tx_svd_trace.txt'
    saved=root/'validation/imported_tx_float_20260924_152146_236522'
    inputs=saved/'matlab_tx'
    reference=root/'validation/multiuser_float_20260924_150540_506860/matlab'
    result={'status':'incomplete','root':str(root),'run_dir':str(out),
            'sqrt_sha256':digest(root/'Cls_sqrt.h'),'sqrt_bit_len':22,
            'numeric_mode':args.numeric_mode,
            'default_config':{'max_sweeps':6,'correlation_tol_shift':8,'absolute_tol_shift':24},'replays':{}}
    result['source_hashes']={str(p.relative_to(root)):digest(p) for p in [*root.glob('*.h'),*(root/'testbench_GWRX_TX').glob('*.h'),root/'testbench_GWRX_TX/GWRXTX_testbench.cpp']}
    print('RUN_DIR='+str(out),flush=True)
    try:
        assert digest(root/'Cls_sqrt.h')==digest(root/'validation/baseline_before_adaptive_svd_20260924/Cls_sqrt.h')
        exe=out/'replay.exe'
        command=[compiler,'-std=c++11','-O1','-g','-Wno-write-strings',str(root/'testbench_GWRX_TX/replay_adaptive_svd.cpp'),'-o',str(exe)]
        run_logged(command,out,out/'replay_build.log',300)
        for mode in ('fixed','float'):
            run=[str(exe),mode,str(trace),str(out/mode),'6','8']
            run_logged(run,out,out/(mode+'_replay.log'),120)
            rows=np.genfromtxt(out/(mode+'_status.csv'),delimiter=',',names=True)
            assert len(rows)==104
            assert np.all(rows['rotations']+rows['skipped']==28*rows['sweeps'])
            data=np.loadtxt(out/(mode+'_matrices.txt')).reshape(-1,8,8,7)
            residuals=[]
            for matrix in data:
                a=matrix[:,:,0]+1j*matrix[:,:,1];u=matrix[:,:,2]+1j*matrix[:,:,3];v=matrix[:,:,4]+1j*matrix[:,:,5];s=matrix[:,:,6]
                residuals.append(float(np.linalg.norm(u@s@v.conj().T-a)/np.linalg.norm(a)))
            r={'matrices':104,'converged':int(rows['converged'].sum()),'hit_max':int(rows['hit_max'].sum()),
               'precision_limited':int(rows['precision_limited'].sum()),'sweeps_mean':float(rows['sweeps'].mean()),
               'sweeps_min':int(rows['sweeps'].min()),'sweeps_max':int(rows['sweeps'].max()),
               'rotations_mean':float(rows['rotations'].mean()),'rotations_max':int(rows['rotations'].max()),
               'pair_checks_mean':float(rows['sweeps'].mean()*28),'reconstruction_residual_median':float(np.median(residuals)),
               'reconstruction_residual_max':max(residuals),'command':run}
            result['replays'][mode]=r;print(mode,r,flush=True)
        output=out/'cmodel'
        for name in ('LostRate','UDModule_Data','BFModule_Data','K2201SyncModule_Data','K2201DemodModule_Data','GWTXModule'):
            (output/name/'case95').mkdir(parents=True)
        exe=out/'GWRXTX.exe'
        build=[compiler,'-std=c++11','-O1','-g','-Wformat=2','-Wno-write-strings','-Wl,--stack,67108864',
               str(root/'testbench_GWRX_TX/GWRXTX_testbench.cpp'),'-o',str(exe)]
        build.insert(1,'-DCMODEL_FIXED_POINT' if args.numeric_mode=='fixed' else '-DCMODEL_FLOAT_POINT')
        result['build_command']=build
        run_logged(build,out,out/'build.log',300)
        command=[str(exe),*(inputs/'cmodel_args.txt').read_text().split(),'--input-root',str(reference),'--output-root',str(output),
                 '--frames','1','--tx-reference-file',str(inputs/'matlab_tx_inputs.txt'),
                 '--tx-bits-file',str(inputs/'tx_bits.txt'),'--tx-user-ids-file',str(inputs/'tx_user_ids.txt')]
        result['run_command']=command
        print('Running '+args.numeric_mode+' RX/TX with imported MATLAB TX inputs',flush=True)
        result['seconds']=run_logged(command,output,out/'run.log',300)
        d=output/'GWTXModule/case95'
        check_imports(inputs/'matlab_tx_inputs.txt',d/'tx_inputs_used.txt')
        assert np.array_equal(np.loadtxt(inputs/'tx_bits.txt'),np.loadtxt(d/'tx_bits_used.txt'))
        cw=load_iq(d/'weights_frame0.txt',8);mw=load_iq(reference/'matlab_tx_weights.txt',8)
        rel=np.linalg.norm(cw-mw,axis=1)/np.linalg.norm(mw,axis=1)
        result['weight_median_percent']=float(np.median(rel)*100);result['weight_max_percent']=float(rel.max()*100)
        result['tx']=compare_iq(load_iq(reference/'matlab_tx_8ant_baseband.txt',8),load_iq(d/'tx_frame0.txt',8)*2**23)
        log=(out/'run.log').read_text()
        assert 'NUMERIC_MODE='+args.numeric_mode in log
        assert 'TX_INPUT_SOURCE=MATLAB users=104' in log
        pattern=r'TX_SVD sweeps=(\d+) rotations=(\d+) skipped=(\d+) converged=(\d+) hit_max=(\d+) precision_limited=(\d+) per_sweep=([\d,]+)'
        tx_stats=[]
        for match in re.findall(pattern,log):
            nums=list(map(int,match[:6]));per=list(map(int,match[6].split(',')))
            assert len(per)==nums[0] and sum(per)==nums[1] and nums[1]+nums[2]==28*nums[0]
            assert nums[0]<=6
            if nums[3]:assert per[-1]==0 and nums[4]==0 and nums[5]==0
            tx_stats.append(dict(zip(('sweeps','rotations','skipped','converged','hit_max','precision_limited'),nums),per_sweep=per))
        assert len(tx_stats)==104
        result['tx_svd_stats']=tx_stats
        result['tx_converged']=sum(s['converged'] for s in tx_stats)
        result['tx_hit_max']=sum(s['hit_max'] for s in tx_stats)
        assert digest(root/'Cls_sqrt.h')==result['sqrt_sha256']
        for p,h in result['source_hashes'].items():assert digest(root/p)==h
        result['plots']=generate_tx_plots(reference/'matlab_tx_8ant_baseband.txt',d/'tx_frame0.txt',out,
            mode=args.numeric_mode,context='同源104用户TX输入；SVD最多6轮')
        result['status']='completed_with_measured_accuracy_limits'
        print('TX weight median/max %',result['weight_median_percent'],result['weight_max_percent'],flush=True)
        print('TX error %',[r['relative_l2_error']*100 for r in result['tx']['per_antenna']],flush=True)
        print('TX SVD converged/max',result['tx_converged'],result['tx_hit_max'],flush=True)
        return 0
    except Exception as exc:
        result.update(status='failed',error=str(exc));print('ERROR: '+str(exc),flush=True);return 1
    finally:save_json(out/'result.json',result)


if __name__=='__main__':sys.exit(main())
