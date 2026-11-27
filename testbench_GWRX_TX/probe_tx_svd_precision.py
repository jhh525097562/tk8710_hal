"""Controlled TX-SVD precision diagnosis in disposable source copies."""
import datetime as dt
import argparse
import json
from pathlib import Path
import shutil
import sys

import numpy as np
from run_matlab_compare import load_iq,compare_iq,run_logged,save_json
from run_imported_tx_compare import check_imports
from run_smoke import digest
from tx_comparison_plots import generate_tx_plots

TRACE = r'''
    {
        std::ofstream trace("tx_svd_trace.txt", std::ios::app);
        trace << std::setprecision(17);
        for(int row=0;row<8;++row) for(int col=0;col<8;++col)
            trace << in[row][col].I << ' ' << in[row][col].Q << ' '
                  << m_U_est[row][col].I << ' ' << m_U_est[row][col].Q << ' '
                  << m_V_est[row][col].I << ' ' << m_V_est[row][col].Q << ' '
                  << m_S_est[row][col] << ' ' << out[row][col].I << ' ' << out[row][col].Q << std::endl;
    }
'''


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--timeout',type=int,default=120)
    parser.add_argument('--variants',nargs='+',default=['svd_fxp','svd_double'],
        choices=['svd_fxp','svd_double','svd_double_10sweeps','svd_double_std_sqrt','svd_double_std_sqrt_10sweeps',
                 'svd_double_std_sqrt_10sweeps_skip_zero'])
    options=parser.parse_args()
    variants=options.variants
    out=root/'validation'/('tx_svd_precision_'+dt.datetime.now().strftime('%Y%m%d_%H%M%S'))
    out.mkdir()
    previous=root/'validation/imported_tx_float_20260924_152146_236522'
    inputs=previous/'matlab_tx'
    reference=root/'validation/multiuser_float_20260924_150540_506860/matlab'
    baseline=json.loads((previous/'comparison.json').read_text(encoding='utf-8'))
    mw=load_iq(reference/'matlab_tx_weights.txt',8)
    mtx=load_iq(reference/'matlab_tx_8ant_baseband.txt',8)
    before_hashes={str(p.relative_to(root)):digest(p) for p in [*root.glob('*.h'),*(root/'testbench_GWRX_TX').glob('*.h'),root/'testbench_GWRX_TX/GWRXTX_testbench.cpp']}
    result={'source_hashes':before_hashes,'inputs':str(inputs),'matlab_reference':str(reference),'runs':{},'status':'incomplete'}
    print('RUN_DIR='+str(out),flush=True)
    try:
        for variant in variants:
            work=out/variant;work.mkdir();source=work/'source';source.mkdir();entry=source/'testbench_GWRX_TX';entry.mkdir()
            for p in root.glob('*.h'):shutil.copy2(p,source/p.name)
            for p in (root/'testbench_GWRX_TX').glob('*.h'):shutil.copy2(p,entry/p.name)
            shutil.copy2(root/'testbench_GWRX_TX/GWRXTX_testbench.cpp',entry/'GWRXTX_testbench.cpp')
            header=source/'Cls_tx_beamforming.h';content=header.read_text(encoding='utf-8')
            if variant!='svd_fxp':
                old='Cls_svd<fxp>          m_svd;'
                assert content.count(old)==1
                content=content.replace(old,'Cls_svd<T>            m_svd;',1)
            marker='#ifdef DUMP_MATLAB_tx_beamforming\n    dump_pinv(in, out);'
            assert content.count(marker)==1
            content=content.replace(marker,TRACE+marker,1)
            header.write_text(content,encoding='utf-8')
            svd_header=source/'Cls_svd.h'
            svd_content=svd_header.read_text(encoding='utf-8')
            if 'std_sqrt' in variant:
                assert 'm_sqrt.sqrt(' in svd_content
                svd_content=svd_content.replace('m_sqrt.sqrt(','std::sqrt(')
            if '10sweeps' in variant:
                assert svd_content.count('m_iter_num = 3;')==1
                svd_content=svd_content.replace('m_iter_num = 3;','m_iter_num = 10;',1)
            if 'skip_zero' in variant:
                marker_zero='m_xiu_sqrt_fxp = std::sqrt(u0);\n    m_QKL = m_wk - m_wl;'
                assert svd_content.count(marker_zero)==1
                svd_content=svd_content.replace(marker_zero,
                    'm_xiu_sqrt_fxp = std::sqrt(u0);\n    if (m_xiu_sqrt_fxp == 0) return;\n    m_QKL = m_wk - m_wl;',1)
            svd_header.write_text(svd_content,encoding='utf-8')
            data=work/'output'
            for name in ('LostRate','UDModule_Data','BFModule_Data','K2201SyncModule_Data','K2201DemodModule_Data','GWTXModule'):
                (data/name/'case95').mkdir(parents=True)
            exe=work/'probe.exe'
            build=['C:/Program Files/mingw64/bin/g++.exe','-DCMODEL_FLOAT_POINT','-std=c++11','-O1','-g','-Wno-write-strings',
                   '-Wl,--stack,67108864',str(entry/'GWRXTX_testbench.cpp'),'-o',str(exe)]
            print(variant+': building/running identical 104-user TX inputs',flush=True)
            run_logged(build,data,work/'build.log',300)
            command=[str(exe),*(inputs/'cmodel_args.txt').read_text().split(),'--input-root',str(reference),'--output-root',str(data),
                     '--frames','1','--tx-reference-file',str(inputs/'matlab_tx_inputs.txt'),'--tx-bits-file',str(inputs/'tx_bits.txt'),
                     '--tx-user-ids-file',str(inputs/'tx_user_ids.txt')]
            seconds=run_logged(command,data,work/'run.log',options.timeout)
            d=data/'GWTXModule/case95'
            check_imports(inputs/'matlab_tx_inputs.txt',d/'tx_inputs_used.txt')
            assert np.array_equal(np.loadtxt(inputs/'tx_bits.txt'),np.loadtxt(d/'tx_bits_used.txt'))
            cw=load_iq(d/'weights_frame0.txt',8);ctx=load_iq(d/'tx_frame0.txt',8)*2**23
            rel=np.linalg.norm(cw-mw,axis=1)/np.linalg.norm(mw,axis=1)
            if variant=='svd_fxp':
                assert digest(d/'weights_frame0.txt')==baseline['runs']['after_wrap']['weights_sha256']
                assert digest(d/'tx_frame0.txt')==baseline['runs']['after_wrap']['waveform_sha256']
            trace=np.loadtxt(data/'tx_svd_trace.txt').reshape(-1,8,8,9)
            matrices=[]
            for k,t in enumerate(trace):
                a=t[:,:,0]+1j*t[:,:,1];u=t[:,:,2]+1j*t[:,:,3];v=t[:,:,4]+1j*t[:,:,5];s=t[:,:,6];p=t[:,:,7]+1j*t[:,:,8]
                ideal=np.linalg.pinv(a)
                matrices.append({'index':k,'singular_values':np.diag(s).tolist(),'numpy_singular_values':np.linalg.svd(a,compute_uv=False).tolist(),
                    'svd_reconstruction_relative_error':float(np.linalg.norm(u@s@v.conj().T-a)/np.linalg.norm(a)),
                    'pinv_relative_error':float(np.linalg.norm(p-ideal)/np.linalg.norm(ideal)),
                    'penrose_residual':float(np.linalg.norm(a@p@a-a)/np.linalg.norm(a)),
                    'v_orthogonality_error':float(np.linalg.norm(v.conj().T@v-np.eye(8)))})
            result['runs'][variant]={'seconds':seconds,'build_command':build,'run_command':command,'staged_header_sha256':digest(header),
                'weight_relative_l2_median':float(np.median(rel)),'weight_relative_l2_max':float(rel.max()),'weight_relative_l2_per_user':rel.tolist(),
                'weights_sha256':digest(d/'weights_frame0.txt'),'waveform_sha256':digest(d/'tx_frame0.txt'),
                'tx':compare_iq(mtx,ctx),'matrices':matrices}
            save_json(out/'comparison.json',result)
            result['runs'][variant]['plots']=generate_tx_plots(reference/'matlab_tx_8ant_baseband.txt',
                d/'tx_frame0.txt',work,mode='float',context='同源104用户TX输入；'+variant)
            print(variant,'weight median/max %',np.median(rel)*100,rel.max()*100,flush=True)
            print(variant,'TX %',[x['relative_l2_error']*100 for x in result['runs'][variant]['tx']['per_antenna']],flush=True)
        for name,h in before_hashes.items():assert digest(root/name)==h
        result['status']='completed';return 0
    except Exception as exc:
        result.update(status='failed',error=str(exc));print(str(exc),flush=True);return 1
    finally:save_json(out/'comparison.json',result)


if __name__=='__main__':sys.exit(main())
