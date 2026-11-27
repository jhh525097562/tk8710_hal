"""Generate a 128-terminal frame and compare identity-matched RX and aggregate TX.

Exit 0: exact payload/user-set and canonical TX equality. Exit 1: measured
differences. Exit 2: workflow error. Known algorithm differences are retained.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import re
import sys

import numpy as np

from run_matlab_compare import stage_hooks, run_logged, save_json, load_iq, compare_iq
from run_smoke import digest
from tx_comparison_plots import generate_tx_plots


def read_rx(path):
    records={}
    duplicates=[]
    for line in path.read_text().splitlines():
        fields=np.asarray([float(x) for x in line.split()])
        if len(fields)<22 or not np.isfinite(fields).all():
            raise ValueError("Invalid C++ RX result row")
        count=int(fields[1])
        if len(fields)!=20+count or count!=28:
            raise ValueError("Unexpected RX PHY byte layout")
        phy=fields[20:].astype(int)
        if not np.array_equal(phy,fields[20:]) or np.any((phy<0)|(phy>255)):
            raise ValueError("Invalid decoded bytes")
        uid=int(phy[0]*255+phy[1])
        entry={'user_id':uid,'rx_index':int(fields[0]),'payload':phy[:-2].tolist(),
               'frequency':float(fields[2]),'pilot_power':float(fields[3]),
               'ah_i':fields[4:20:2].tolist(),'ah_q':fields[5:20:2].tolist()}
        if uid in records: duplicates.append(uid)
        records[uid]=entry
    return records,duplicates


def match_rx(matlab,actual,transmitted):
    common=sorted(matlab.keys() & actual.keys())
    details=[]
    for uid in common:
        m,c=matlab[uid],actual[uid]
        ma=np.asarray(m['ah_i']).ravel()+1j*np.asarray(m['ah_q']).ravel()
        ca=np.asarray(c['ah_i'])+1j*np.asarray(c['ah_q'])
        details.append({'user_id':uid,'payload_equal':m['payload']==c['payload'],
            'matlab_payload_matches_transmitted':m['payload']==transmitted[uid],
            'cmodel_payload_matches_transmitted':c['payload']==transmitted[uid],
            'frequency_error_hz':c['frequency']-m['frequency'],
            'ah_relative_l2':float(np.linalg.norm(ca-ma)/np.linalg.norm(ma)) if np.linalg.norm(ma) else None,
            'pilot_power_error':c['pilot_power']-m['pilot_power']})
    return {'matlab_users':len(matlab),'cmodel_users':len(actual),'common_users':len(common),
            'only_matlab':sorted(matlab.keys()-actual.keys()),'only_cmodel':sorted(actual.keys()-matlab.keys()),
            'missing_matlab':sorted(transmitted.keys()-matlab.keys()),'missing_cmodel':sorted(transmitted.keys()-actual.keys()),
            'payload_equal_common_users':sum(d['payload_equal'] for d in details),
            'matlab_payload_errors':[u for u,v in matlab.items() if transmitted.get(u)!=v['payload']],
            'cmodel_payload_errors':[u for u,v in actual.items() if transmitted.get(u)!=v['payload']],
            'details':details}


def main():
    if hasattr(sys.stdout,'reconfigure'):
        sys.stdout.reconfigure(errors='backslashreplace')
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--users',type=int,default=128,choices=range(1,129))
    ap.add_argument('--reference',type=Path,default=Path('D:/Users/HJH/Desktop/HJH/K2301Cmodel/TKG810MatlabPlatform'))
    ap.add_argument('--matlab',default='D:/Matlab2021a/bin/matlab.exe')
    ap.add_argument('--compiler',default='C:/Program Files/mingw64/bin/g++.exe')
    ap.add_argument('--timeout',type=int,default=3600)
    ap.add_argument('--reuse-generated',type=Path)
    args=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=root/'validation'/('multiuser_float_'+dt.datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    out.mkdir(parents=True)
    result={'status':'incomplete','generated_users':args.users,'numeric_mode':'float',
            'numeric_mode_limit':'TX internal SVD and RX Viterbi state metrics remain fxp in the current model',
            'run_dir':str(out),'frames':1,'known_phase_wrap_issue_preserved':True}
    print('RUN_DIR='+str(out),flush=True)
    try:
        if args.reuse_generated:
            mdir=args.reuse_generated.resolve()
            result['reused_matlab_data']=str(mdir)
        else:
            mdir=out/'matlab'; mdir.mkdir()
            hooks=out/'matlab_hooks'
            result['hooks']=stage_hooks(args.reference,hooks)
            result['matlab_source_sha256']={str(p.relative_to(args.reference)):digest(p) for p in args.reference.rglob('*.m')}
            result['workbook_sha256']=digest(args.reference/'TestCaseListNew3.xlsx')
            quote=lambda p: "'"+Path(p).resolve().as_posix().replace("'","''")+"'"
            expr='addpath('+quote(root/'testbench_GWRX_TX')+');matlab_one_frame('+','.join(quote(p) for p in (args.reference,mdir,hooks))+','+str(args.users)+');'
            command=[args.matlab,'-batch',expr]
            result['matlab_command']=command
            print(f'MATLAB: generating {args.users} real terminals in one frame',flush=True)
            result['matlab_seconds']=run_logged(command,mdir,out/'matlab.log',args.timeout)
        meta=json.loads((mdir/'matlab_results.json').read_text(encoding='utf-8'))
        if meta['generated_user_count']!=args.users: raise ValueError('Generated user count does not match request')
        result['matlab_metadata']=meta
        mrecords=meta['rx_records']
        if isinstance(mrecords,dict): mrecords=[mrecords]
        mrx={int(x['user_id']):x for x in mrecords}
        if len(mrx)!=meta['rx_user_count']: raise ValueError('MATLAB duplicate IDs')
        parameters=(mdir/'cmodel_args.txt').read_text().split()
        if len(parameters)!=91 or int(parameters[14])!=args.users: raise ValueError('Invalid case parameters')
        result['inputs_sha256']={p:digest(mdir/p) for p in ('Cmodel_DataIn/case95/UD_InData0.txt','tx_bits.txt','tx_user_ids.txt','cmodel_args.txt')}
        cdir=out/'cmodel'
        for name in ('LostRate','UDModule_Data','BFModule_Data','K2201SyncModule_Data','K2201DemodModule_Data','GWTXModule'):
            (cdir/name/'case95').mkdir(parents=True)
        exe=out/'GWRXTX_float.exe'
        source=root/'testbench_GWRX_TX/GWRXTX_testbench.cpp'
        build=[args.compiler,'-DCMODEL_FLOAT_POINT','-std=c++11','-O1','-g','-Wformat=2','-Wno-write-strings',
               '-Wl,--stack,67108864',str(source),'-o',str(exe)]
        result['build_command']=build
        tracked=[*root.glob('*.h'),source,root/'testbench_GWRX_TX/testbench_options.h',
                 root/'testbench_GWRX_TX/matlab_one_frame.m',Path(__file__).resolve()]
        result['source_sha256']={str(p.relative_to(root)):digest(p) for p in tracked}
        print(f'MATLAB decoded {len(mrx)}/{args.users}; building C++ float',flush=True)
        run_logged(build,cdir,out/'build.log',300)
        command=[str(exe),*parameters,'--input-root',str(mdir),'--output-root',str(cdir),'--frames','1',
                 '--tx-bits-file',str(mdir/'tx_bits.txt'),'--tx-user-ids-file',str(mdir/'tx_user_ids.txt')]
        result['cmodel_command']=command
        result['cmodel_seconds']=run_logged(command,cdir,out/'cmodel.log',args.timeout)
        log=(out/'cmodel.log').read_text()
        if 'NUMERIC_MODE=float (double)' not in log or 'RxFinished' not in log: raise ValueError('Incomplete or wrong numeric mode')
        crx,duplicates=read_rx(cdir/'K2201DemodModule_Data/case95/rx_frame0.txt')
        payload=np.asarray(meta['uplink_payload']).reshape(args.users,-1)
        transmitted={i+1:payload[i].tolist() for i in range(args.users)}
        result['rx']=match_rx(mrx,crx,transmitted)
        result['rx']['duplicate_cmodel_ids']=duplicates
        result['rx']['crc_clean_decode_events']=len(re.findall(r'RX_DECODE .*segments=1 crc_errors=0',log))
        save_json(out/'rx_comparison.json',result['rx'])
        print('RX:',{k:v for k,v in result['rx'].items() if k!='details'},flush=True)
        if not crx: raise ValueError('No C++ RX users; TX comparison unavailable')
        if duplicates: raise ValueError('Ambiguous duplicate C++ IDs; refusing weight matching')
        mtx=load_iq(mdir/'matlab_tx_8ant_baseband.txt',8)
        ctx=load_iq(cdir/'GWTXModule/case95/tx_frame0.txt',8)
        result['tx_raw']=compare_iq(mtx,ctx)
        result['tx_canonical']=compare_iq(mtx,ctx*meta['cmodel_tx_scale'])
        result['tx_canonical']['cmodel_scale']=meta['cmodel_tx_scale']
        result['tx_canonical']['same_user_set']=mrx.keys()==crx.keys()
        result['tx_canonical']['scope']='Each side transmits for its own successfully decoded users; differing user sets also affect aggregate waveform and power allocation'
        mw=load_iq(mdir/'matlab_tx_weights.txt',8)
        cw=load_iq(cdir/'GWTXModule/case95/weights_frame0.txt',8)
        mi={int(uid):k for k,uid in enumerate(np.asarray(meta['tx_user_ids']).ravel())}
        result['weight_errors']=[]
        for uid in sorted(mrx.keys()&crx.keys()):
            a,b=mw[mi[uid]],cw[crx[uid]['rx_index']]
            result['weight_errors'].append({'user_id':uid,'relative_l2':float(np.linalg.norm(b-a)/np.linalg.norm(a)),
                                            'max_abs_error':float(np.max(abs(b-a)))})
        mappings=re.findall(r'TX_PAYLOAD_MAP rx_index=(\d+) user_id=(\d+) bank_row=(\d+)',log)
        if len(mappings)!=len(crx) or any(int(uid)!=int(bank)+1 for _,uid,bank in mappings):
            raise AssertionError('TX payload identity mapping failed')
        result['tx_payload_mapping_verified']=True
        result['waveform_sha256']={'matlab_baseband':digest(mdir/'matlab_tx_8ant_baseband.txt'),
                                  'matlab_rf':digest(mdir/'matlab_tx_8ant_rf.txt'),
                                  'cmodel':digest(cdir/'GWTXModule/case95/tx_frame0.txt')}
        same_rx=mrx.keys()==crx.keys() and all(d['payload_equal'] for d in result['rx']['details'])
        result['rx']['same_user_set_and_payload']=same_rx
        exact=same_rx and result['tx_canonical']['exact_equal']
        result['plots']=generate_tx_plots(mdir/'matlab_tx_8ant_baseband.txt',
            cdir/'GWTXModule/case95/tx_frame0.txt',out,mode='float',scale=meta['cmodel_tx_scale'],
            sample_rate=meta['tx_baseband_rate'],context=f"{args.users}用户输入；MATLAB RX {len(mrx)} / C_model RX {len(crx)}，各自RX驱动TX")
        result['status']='exact_match' if exact else 'completed_with_differences'
        print('RX same user set and payload:',same_rx,flush=True)
        for row in result['tx_canonical']['per_antenna']: print(row,flush=True)
        return 0 if exact else 1
    except Exception as exc:
        result.update(status='workflow_failed',error=str(exc))
        print('ERROR: '+str(exc),flush=True)
        return 2
    finally:
        save_json(out/'comparison.json',result)


if __name__=='__main__': sys.exit(main())
