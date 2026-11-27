"""Use identical MATLAB PHYGWTx inputs in C++ and measure float wrap correction."""
import argparse
import datetime as dt
import json
from pathlib import Path
import shutil
import sys

import numpy as np
from run_matlab_compare import run_logged, save_json, load_iq, compare_iq
from run_smoke import digest
from tx_comparison_plots import generate_tx_plots


def check_imports(exported,used):
    lhs=exported.read_text().split();rhs=used.read_text().split()
    if lhs[0]!=rhs[0] or len(lhs)!=len(rhs): raise AssertionError('Imported TX schema mismatch')
    if not np.array_equal(np.asarray(lhs[1:],float),np.asarray(rhs[1:],float)):
        raise AssertionError('TX channel/config inputs differ after import')


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    ap=argparse.ArgumentParser(description=__doc__)
    root=Path(__file__).resolve().parents[1]
    ap.add_argument('--matlab-data',type=Path,default=root/'validation/multiuser_float_20260924_150540_506860/matlab')
    ap.add_argument('--timeout',type=int,default=1800)
    args=ap.parse_args()
    original=args.matlab_data.resolve()
    out=root/'validation'/('imported_tx_float_'+dt.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    out.mkdir()
    result={'status':'incomplete','source_matlab_data':str(original),'run_dir':str(out),
            'numeric_mode':'float (double), internal TX SVD remains fxp', 'scale':2**23}
    print('RUN_DIR='+str(out),flush=True)
    try:
        exported=out/'matlab_tx';exported.mkdir()
        quote=lambda p:"'"+p.as_posix().replace("'","''")+"'"
        command=['D:/Matlab2021a/bin/matlab.exe','-batch',
            'addpath('+quote(root/'testbench_GWRX_TX')+');export_matlab_tx_inputs('+quote(original/'matlab_one_frame.mat')+','+quote(exported)+');']
        result['matlab_export_command']=command
        result['source_frame_sha256']=digest(original/'matlab_one_frame.mat')
        result['matlab_export_seconds']=run_logged(command,exported,out/'matlab_export.log',args.timeout)
        meta=json.loads((exported/'export.json').read_text(encoding='utf-8'))
        count=meta['tx_user_count'];result['tx_user_count']=count;result['tx_user_ids']=meta['tx_user_ids']
        mtx=load_iq(original/'matlab_tx_8ant_baseband.txt',8)
        mw=load_iq(original/'matlab_tx_weights.txt',8)
        result['export_hashes']={p.name:digest(p) for p in exported.iterdir() if p.is_file()}
        result['source_hashes']={str(p.relative_to(root)):digest(p) for p in [*root.glob('*.h'),
            root/'testbench_GWRX_TX/GWRXTX_testbench.cpp',* (root/'testbench_GWRX_TX').glob('*.h'),
            Path(__file__).resolve(),root/'testbench_GWRX_TX/export_matlab_tx_inputs.m']}
        # A control build varies only the previously saved interpolation header.
        control=out/'before_source';control.mkdir();(control/'testbench_GWRX_TX').mkdir()
        for header in root.glob('*.h'):shutil.copy2(header,control/header.name)
        for header in (root/'testbench_GWRX_TX').glob('*.h'):
            shutil.copy2(header,control/'testbench_GWRX_TX'/header.name)
        shutil.copy2(root/'testbench_GWRX_TX/GWRXTX_testbench.cpp',control/'testbench_GWRX_TX/GWRXTX_testbench.cpp')
        before_header=root/'validation/baseline_before_matlab_tx_import_20260924/Cls_tx_us_2_1.h'
        shutil.copy2(before_header,control/'Cls_tx_us_2_1.h')
        result['before_header_sha256']=digest(before_header)
        result['runs']={}
        for label,source in (('before_wrap',control/'testbench_GWRX_TX/GWRXTX_testbench.cpp'),
                             ('after_wrap',root/'testbench_GWRX_TX/GWRXTX_testbench.cpp')):
            output=out/label
            for name in ('LostRate','UDModule_Data','BFModule_Data','K2201SyncModule_Data','K2201DemodModule_Data','GWTXModule'):
                (output/name/'case95').mkdir(parents=True)
            exe=output/'GWRXTX.exe'
            build=['C:/Program Files/mingw64/bin/g++.exe','-DCMODEL_FLOAT_POINT','-std=c++11','-O1','-g','-Wformat=2',
                   '-Wno-write-strings','-Wl,--stack,67108864',str(source),'-o',str(exe)]
            print(f'{label}: build/run with {count} identical MATLAB TX users',flush=True)
            run_logged(build,output,output/'build.log',300)
            run=[str(exe),*(exported/'cmodel_args.txt').read_text().split(),'--input-root',str(original),
                '--output-root',str(output),'--frames','1','--tx-reference-file',str(exported/'matlab_tx_inputs.txt'),
                '--tx-bits-file',str(exported/'tx_bits.txt'),'--tx-user-ids-file',str(exported/'tx_user_ids.txt')]
            seconds=run_logged(run,output,output/'run.log',args.timeout)
            log=(output/'run.log').read_text()
            if f'TX_INPUT_SOURCE=MATLAB users={count}' not in log or f'TX_FRAME frame=0 users={count}' not in log:
                raise AssertionError('TX did not use the imported user count')
            if 'NUMERIC_MODE=float (double)' not in log:raise AssertionError('Wrong numeric mode')
            data=output/'GWTXModule/case95'
            check_imports(exported/'matlab_tx_inputs.txt',data/'tx_inputs_used.txt')
            bits=np.loadtxt(exported/'tx_bits.txt',dtype=int)
            if not np.array_equal(bits,np.loadtxt(data/'tx_bits_used.txt',dtype=int)):
                raise AssertionError('Actual TX payload differs')
            cw=load_iq(data/'weights_frame0.txt',8)
            wave=load_iq(data/'tx_frame0.txt',8)
            if cw.shape!=mw.shape or wave.shape!=mtx.shape: raise AssertionError('TX shape mismatch')
            weight_rel=np.linalg.norm(cw-mw,axis=1)/np.linalg.norm(mw,axis=1)
            metrics=compare_iq(mtx,wave*2**23)
            result['runs'][label]={'tx_inputs_exact':True,'tx_payload_exact':True,'seconds':seconds,
                'build_command':build,'run_command':run,'waveform_sha256':digest(data/'tx_frame0.txt'),
                'weights_sha256':digest(data/'weights_frame0.txt'),'tx':metrics,
                'weight_relative_l2_per_user':weight_rel.tolist(),
                'weight_relative_l2_median':float(np.median(weight_rel)),
                'weight_relative_l2_max':float(np.max(weight_rel))}
            print(label,'TX relative L2 %:',[round(x['relative_l2_error']*100,5) for x in metrics['per_antenna']],flush=True)
            result['runs'][label]['plots']=generate_tx_plots(original/'matlab_tx_8ant_baseband.txt',
                data/'tx_frame0.txt',output,mode='float',context=f'同源{count}用户TX输入；{label}')
        for name,h in result['source_hashes'].items():
            if digest(root/name)!=h:raise AssertionError('Source changed during run: '+name)
        result['matlab_waveform_sha256']=digest(original/'matlab_tx_8ant_baseband.txt')
        result['status']='exact_match' if result['runs']['after_wrap']['tx']['exact_equal'] else 'completed_with_differences'
        return 0 if result['status']=='exact_match' else 1
    except Exception as exc:
        result.update(status='workflow_failed',error=str(exc));print('ERROR: '+str(exc),flush=True);return 2
    finally:save_json(out/'comparison.json',result)


if __name__=='__main__':sys.exit(main())
