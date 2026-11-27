"""Replay phase interpolation and test input wrapping in an isolated C++ copy.

Requires diagnose_tx_phase.m outputs; never edits production algorithm sources.
"""
import csv
import json
from pathlib import Path
import subprocess

import numpy as np

from run_matlab_compare import load_iq, compare_iq
from run_smoke import digest


def upsample(phase, wrap_midpoints=False):
    stages=[]
    z=np.r_[phase,2*phase[-1]-phase[-2]]
    for _ in range(3):
        a,b=z[:-1],z[1:]
        mid=(a+b+np.where(a-b>1,2,np.where(b-a>1,-2,0)))/2
        if wrap_midpoints:
            mid=np.where(mid>1,mid-2,np.where(mid< -1,mid+2,mid))
        y=np.empty(2*len(z)-1)
        y[::2]=z; y[1::2]=mid; z=y
        stages.append(z)
    return z,stages


def main():
    root=Path(__file__).resolve().parents[1]
    diag=root/'validation/tx_phase_diagnosis_20260924'
    baseline=root/'validation/matlab_compare_float_20260923_180530_159111'
    evidence=json.loads((baseline/'comparison.json').read_text(encoding='utf-8'))
    ref=Path(evidence['reused_matlab_data'])
    module=baseline/'cmodel/GWTXModule/case95'
    mphase=np.loadtxt(diag/'matlab_mod_phase.txt')
    # The old debug file contains earlier intermediate dumps; the final block
    # is the actual 204 symbols * 16 phase samples written by go() at line 498.
    cphase=np.loadtxt(module/'o_sigtx.txt')[-len(mphase):]
    canonical=(cphase+1)%2-1
    m_up=np.r_[-np.ones(384),np.loadtxt(diag/'matlab_up_phase.txt')]
    before, stages=upsample(np.r_[-np.ones(48),cphase])
    after,_=upsample(np.r_[-np.ones(48),canonical])
    reference_wave=load_iq(ref/'matlab_tx_8ant_baseband.txt',8)
    actual_wave=load_iq(module/'tx_frame0.txt',8)*2**23
    assert digest(module/'tx_frame0.txt')==evidence['waveform_sha256']['cmodel']
    weights=load_iq(module/'weights_frame0.txt',8)[0]
    user=actual_wave[:,0]/weights[0]
    n=np.arange(len(user))
    frequency=round(evidence['rx']['cmodel_frequency'])
    ddc_step=2*frequency*274878*2**-35
    reconstructed=np.exp(1j*np.pi*(before+n*ddc_step))
    m_user=load_iq(ref/'matlab_tx_user_wave.txt')[:,0]
    m_reconstructed=np.exp(1j*np.pi*m_up)*np.exp(2j*np.pi*n*frequency/125000)
    errors_before=abs(np.exp(1j*np.pi*before)-np.exp(1j*np.pi*m_up))
    errors_after=abs(np.exp(1j*np.pi*after)-np.exp(1j*np.pi*m_up))
    observed_bad=abs(actual_wave[:,0]-reference_wave[:,0]) > .01*np.sqrt(np.mean(abs(reference_wave[:,0])**2))
    predicted_bad=errors_before>.01
    assert np.array_equal(observed_bad,predicted_bad)
    assert np.max(abs(user-reconstructed))<1e-10
    assert np.max(abs(m_user-m_reconstructed))<1e-10
    assert errors_after.max()<1e-10
    groups=np.split(np.flatnonzero(predicted_bad),np.flatnonzero(np.diff(np.flatnonzero(predicted_bad))>1)+1)
    summary={
        'source_run':str(baseline),'matlab_reference':str(ref),
        'cmodel_waveform_sha256':digest(module/'tx_frame0.txt'),
        'matlab_phase_sha256':digest(diag/'matlab_mod_phase.txt'),
        'production_hashes':{str(p.relative_to(root)):digest(p) for p in [*root.glob('*.h'),root/'testbench_GWRX_TX/GWRXTX_testbench.cpp']},
        'canonical_mod_phase_max_error':float(np.max(abs(canonical-mphase))),
        'original_mod_complex_max_error':float(np.max(abs(np.exp(1j*np.pi*cphase)-np.exp(1j*np.pi*mphase)))),
        'reconstructed_cmodel_user_max_error':float(np.max(abs(user-reconstructed))),
        'reconstructed_matlab_user_max_error':float(np.max(abs(m_user-m_reconstructed))),
        'original_upsampling_bad_count':int(predicted_bad.sum()),
        'predicted_bad_indices_equal_observed':True,
        'bad_groups':[[int(g[0]),int(g[-1])] for g in groups],
        'wrapped_input_upsampling_max_complex_error':float(errors_after.max()),
        'wrapped_input_upsampling_bad_count':int(np.count_nonzero(errors_after>.01)),
        'local_input':{'cmodel':cphase[46:48].tolist(),'matlab':mphase[46:48].tolist()},
        'ddc_frequency_hz':frequency,
        'ddc_phase_error_end_radians':float((ddc_step-2*frequency/125000)*(len(n)-1)*np.pi),
    }
    np.savetxt(diag/'phase_input_comparison.csv',np.c_[np.arange(len(cphase)),cphase,canonical,mphase],delimiter=',',
               header='index,cmodel_raw,cmodel_modulo_2,matlab',comments='')
    np.savetxt(diag/'local_phase_output.csv',np.c_[n[748:765],before[748:765],after[748:765],m_up[748:765]],delimiter=',',
               header='output_index,original,wrapped_input,matlab',comments='')
    # Controlled intervention in a separate entry copy; all original headers
    # and algorithms are read directly, without changing them on disk.
    source=root/'testbench_GWRX_TX/GWRXTX_testbench.cpp'
    content=source.read_text(encoding='utf-8')
    marker='m_tx_mod.go(fsk,dqpsk,out);'
    assert content.count(marker)==1
    content=content.replace(marker,marker+'\n                    for (int k=0; k<m_tx_mod.m_outlen; ++k) {\n'
        '                        out[k] -= 2.0 * std::floor((out[k] + 1.0) / 2.0);\n                    }',1)
    content=content.replace('#include "../','#include "'+root.as_posix()+'/')
    content=content.replace('#include "testbench_options.h"','#include "'+(root/'testbench_GWRX_TX/testbench_options.h').as_posix()+'"')
    staged=diag/'GWRXTX_input_wrap_probe.cpp'
    staged.write_text(content,encoding='utf-8')
    output=diag/'probe_output'
    if output.exists(): raise RuntimeError('Probe output already exists; retain old evidence and choose a new directory')
    for folder in ('LostRate','UDModule_Data','BFModule_Data','K2201SyncModule_Data','K2201DemodModule_Data','GWTXModule'):
        (output/folder/'case95').mkdir(parents=True)
    exe=diag/'GWRXTX_input_wrap_probe.exe'
    command=['C:/Program Files/mingw64/bin/g++.exe','-DCMODEL_FLOAT_POINT','-std=c++11','-O1','-g','-Wno-write-strings',
             '-Wl,--stack,67108864',str(staged),'-o',str(exe)]
    with (diag/'probe_build.log').open('w') as log:
        subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
    run=[str(exe),*(ref/'cmodel_args.txt').read_text().split(),'--input-root',str(ref),'--output-root',str(output),
         '--frames','1','--tx-bits-file',str(ref/'tx_bits.txt')]
    with (diag/'probe_run.log').open('w') as log:
        subprocess.run(run,cwd=output,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
    wave=load_iq(output/'GWTXModule/case95/tx_frame0.txt',8)*2**23
    summary['probe_build_command']=command
    summary['probe_run_command']=run
    summary['probe_source_sha256']=digest(staged)
    summary['probe_tx']=compare_iq(reference_wave,wave)
    summary['probe_large_error_count_per_antenna']=np.count_nonzero(abs(wave-reference_wave)>.01*np.sqrt(np.mean(abs(reference_wave)**2,axis=0)),axis=0).tolist()
    for relative,original in summary['production_hashes'].items():
        assert digest(root/relative)==original,relative
    (diag/'diagnosis.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('production_hashes','probe_build_command','probe_run_command')},ensure_ascii=False,indent=2))


if __name__=='__main__': main()
